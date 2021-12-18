/* charmlite completion detection demo
 *
 * ( completion detection is important
 *   for hypercomm collections, hence
 *   giving priority to this! )
 *
 * author: j. szaday <szaday2@illinois.edu>
 */

#include <cmk.hh>

// a callback to resume the main thread
void resume_main_(cmk::message*);

// a chare that uses an int for its index
class completion : public cmk::chare<completion, int> {
 public:
  struct count;
  using count_message = cmk::data_message<count>;
  using detection_message = cmk::data_message<
      std::tuple<cmk::collection_index_t, cmk::callback<cmk::message>>>;

  struct status {
    detection_message* msg;
    std::int64_t lcount;
    bool complete;

    status(detection_message* msg_) : msg(msg_), lcount(0), complete(false) {}
  };

  struct count {
    cmk::collection_index_t target;
    std::int64_t gcount;

    count(cmk::collection_index_t target_, std::int64_t gcount_)
        : target(target_), gcount(gcount_) {}

    // used by the cmk::add operator
    count& operator+=(const count& other) {
      this->gcount += other.gcount;
      return *this;
    }
  };

  cmk::collection_map<status> statii;

  completion(void) = default;

  // obtain the completion status of a collection
  // (setting a callback message if one isn't present)
  status& get_status(cmk::collection_index_t idx,
                     detection_message* msg = nullptr) {
    auto find = this->statii.find(idx);
    if (find == std::end(this->statii)) {
      find = this->statii.emplace(idx, msg).first;
    } else if (msg) {
      find->second.msg = msg;
    }
    return find->second;
  }

  // starts completion detection on _this_ pe
  // (all pes need to start it for it to complete)
  void start_detection(detection_message* msg) {
    auto& val = msg->value();
    auto& idx = std::get<0>(val);
    auto& status = this->get_status(idx, msg);
    if (status.complete) {
      // the root invokes the callback
      if (this->index() == 0) {
        std::get<1>(val).send(msg);
      }
      // and, just to be safe, reset our status!
      new (&status) completion::status(nullptr);
    } else {
      // contribute to the all_reduce with other participants
      auto cb = this->collection_proxy()
                    .callback<count_message, &completion::receive_count_>();
      auto* count = new count_message(idx, status.lcount);
      this->element_proxy()
          .contribute<count_message, cmk::add<typename count_message::type>>(
              count, cb);
    }
  }

  // produce one or more events
  void produce(cmk::collection_index_t idx, std::int64_t n = 1) {
    this->get_status(idx).lcount += n;
  }

  // consume one or more events
  void consume(cmk::collection_index_t idx, std::int64_t n = 1) {
    this->produce(idx, -n);
  }

 private:
  // receive the global-count from the all-reduce
  // and update the status accordingly
  void receive_count_(count_message* msg) {
    auto& gcount = msg->value();
    auto& status = this->get_status(gcount.target);
    status.complete = (gcount.gcount == 0);
    this->start_detection(status.msg);
    cmk::message::free(msg);
  }
};

struct test : cmk::chare<test, int> {
  cmk::group_proxy<completion> detector;
  bool detection_started_;

  test(cmk::data_message<cmk::group_proxy<completion>>* msg)
      : detector(msg->value()) {}

  void produce(cmk::message* msg) {
    // reset completion detection status
    // (only at "root" element)
    this->detection_started_ = (this->index() != 0);
    // check whether we have a local branch
    auto* local = detector.local_branch();
    if (local == nullptr) {
      auto elt = this->element_proxy();
      // put the message back to if we don't
      elt.send<cmk::message, &test::produce>(msg);
    } else {
      // each pe will expect a message from each pe (inclusive)
      CmiPrintf("%d> producing %d value(s)...\n", CmiMyPe(), CmiNumPes());
      local->produce(this->collection(), CmiNumPes());
      // so send the messages
      cmk::group_proxy<test> col(this->collection());
      col.broadcast<cmk::message, &test::consume>(msg);
    }
  }

  void consume(cmk::message* msg) {
    auto* local = detector.local_branch();
    if (local == nullptr) {
      auto elt = this->element_proxy();
      // put the message back to await local branch creation
      elt.send<cmk::message, &test::consume>(msg);
    } else {
      // indicate that we received an expected message
      CmiPrintf("%d> consuming a value...\n", CmiMyPe());
      local->consume(this->collection());

      // start completion detection if we haven't already
      if (!detection_started_) {
        // goal : wake up the main pe!
        auto cb = cmk::callback<cmk::message>::construct<resume_main_>(0);
        auto* dm = new completion::detection_message(this->collection(), cb);
        // (each pe could start its own completion detection
        //  but this checks that broadcasts are working!)
        detector.broadcast<completion::detection_message,
                           &completion::start_detection>(dm);

        detection_started_ = true;
      }

      delete msg;
    }
  }
};

CthThread th;

void resume_main_(cmk::message* msg) { CthAwaken(th); }

int main(int argc, char** argv) {
  cmk::initialize(argc, argv);
  if (CmiMyNode() == 0) {
    // assert that this test will not explode
    th = CthSelf();
    CmiAssert(th && CthIsSuspendable(th));
    // establish detector and participant groups
    auto detector = cmk::group_proxy<completion>::construct();
    auto* dm = new cmk::data_message<decltype(detector)>(detector);
    auto elts = cmk::group_proxy<test>::construct(dm);
    auto nIts = CmiNumPes();
    for (auto it = 0; it < nIts; it++) {
      // send each element a "produce" message to start the process
      elts.broadcast<cmk::message, &test::produce>(new cmk::message);
      // sleep until detection completes
      CthSuspend();
      CmiPrintf("main> iteration %d complete!\n", it + 1);
    }
    // all done...
    cmk::exit();
  }
  cmk::finalize();
  return 0;
}
