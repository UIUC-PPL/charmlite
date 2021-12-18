/* charmlite completion detection demo
 *
 * ( completion detection is important
 *   for hypercomm collections, hence
 *   giving priority to this! )
 *
 * author: j. szaday <szaday2@illinois.edu>
 */

#include <cmk.hh>

// a chare that uses an int for its index
class completion : public cmk::chare<completion, int> {
 public:
  struct count;
  using detection_message =
      cmk::data_message<std::tuple<cmk::collection_index_t, cmk::callback>>;
  using count_message = cmk::data_message<count>;

  struct status {
    detection_message* msg;
    std::int64_t lcount;
    bool complete;

    status(detection_message* msg_) : msg(msg_), lcount(0), complete(false) {}
  };

  struct count {
    cmk::collection_index_t detector;
    cmk::collection_index_t target;
    std::int64_t gcount;

    count(cmk::collection_index_t detector_, cmk::collection_index_t target_,
          std::int64_t gcount_)
        : detector(detector_), target(target_), gcount(gcount_) {}

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
      // invoke the callback when we complete
      std::get<1>(val).send(msg);
    } else {
      // contribute to the all_reduce with other participants
      auto cb = cmk::callback::construct<receive_count_>(cmk::all);
      auto* count = new count_message(this->collection(), idx, status.lcount);
      this->element_proxy().contribute<cmk::add<typename count_message::type>>(
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
  static void receive_count_(cmk::message* msg) {
    auto& gcount = static_cast<count_message*>(msg)->value();
    auto* self = cmk::lookup(gcount.detector)->lookup<completion>(CmiMyPe());
    auto& status = self->get_status(gcount.target);
    status.complete = (gcount.gcount == 0);
    self->start_detection(status.msg);
    cmk::message::free(msg);
  }
};

struct test : cmk::chare<test, int> {
  cmk::group_proxy<completion> detector;
  bool detection_started_;

  test(cmk::data_message<cmk::group_proxy<completion>>* msg)
      : detector(msg->value()), detection_started_(false) {}

  void produce(cmk::message* msg) {
    auto* local = detector.local_branch();
    if (local == nullptr) {
      auto elt = this->element_proxy();
      // put the message back to await local branch creation
      elt.send<cmk::message, &test::produce>(msg);
    } else {
      // each pe will expect a message from each pe (inclusive)
      CmiPrintf("%d> producing %d value(s)...\n", CmiMyPe(), CmiNumPes());
      detector.local_branch()->produce(this->collection(), CmiNumPes());
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

      // start completion detection at "root" if we haven't already
      if (!detection_started_ && (this->index() == 0)) {
        // call exit on all pes when we complete!
        auto cb = cmk::callback::construct<cmk::exit>(cmk::all);
        // (each pe could start its own completion detection
        //  but this checks that broadcasts are working!)
        detector.broadcast<completion::detection_message,
                           &completion::start_detection>(
            new completion::detection_message(this->collection(), cb));

        detection_started_ = true;
      }

      delete msg;
    }
  }
};

int main(int argc, char** argv) {
  cmk::initialize(argc, argv);
  if (CmiMyNode() == 0) {
    // establish detector and participant groups
    auto detector = cmk::group_proxy<completion>::construct();
    auto* dm = new cmk::data_message<decltype(detector)>(detector);
    auto elts = cmk::group_proxy<test>::construct(dm);
    // send each element a "produce" message to start the process
    elts.broadcast<cmk::message, &test::produce>(new cmk::message);
  }
  cmk::finalize();
  return 0;
}
