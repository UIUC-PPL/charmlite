#ifndef CHARMLITE_CORE_COMPLETION_HPP
#define CHARMLITE_CORE_COMPLETION_HPP

#include <charmlite/core/common.hpp>
#include <charmlite/core/message.hpp>
#include <charmlite/core/proxy.hpp>

#include <charmlite/algorithms/reduction.hpp>

namespace cmk {
    // a chare that uses an int for its index
    class completion : public chare<completion, int>
    {
    public:
        struct count;
        using count_message = data_message<count>;
        using detection_message =
            data_message<std::tuple<collection_index_t, callback<message>>>;

        struct status
        {
            message_ptr<detection_message> msg;
            std::int64_t lcount;
            collective_id_t tag;
            bool complete;

            status(message_ptr<detection_message>&& msg_)
              : msg(std::move(msg_))
              , lcount(0)
              , complete(false)
            {
            }
        };

        struct count
        {
            collection_index_t target;
            std::int64_t gcount;

            count(collection_index_t target_, std::int64_t gcount_)
              : target(target_)
              , gcount(gcount_)
            {
            }

            // used by the add operator
            count& operator+=(const count& other)
            {
                CmiEnforce(this->target == other.target);
                this->gcount += other.gcount;
                return *this;
            }
        };

        collection_map<status> statii;

        completion(void) = default;

        // obtain the completion status of a collection
        // (setting a callback message if one isn't present)
        status& get_status(
            collection_index_t const& idx, message_ptr<detection_message>& msg)
        {
            auto find = this->statii.find(idx);
            if (find == std::end(this->statii))
            {
                find = this->statii.emplace(idx, std::move(msg)).first;
                // TODO ( this is FRAGILE -- it assumes no hash collisions which
                //        simply isn't a realistic assumption at scale! )
                find->second.tag = compress(idx);
            }
            else if (msg)
            {
                find->second.msg = std::move(msg);
            }
            return find->second;
        }

        // starts completion detection on _this_ pe
        // (all pes need to start it for it to complete)
        void start_detection(message_ptr<detection_message>&& msg)
        {
            auto& val = msg->value();
            auto& idx = std::get<0>(val);
            auto& status = this->get_status(idx, msg);
            if (status.complete)
            {
                // the root invokes the callback
                if (this->index() == 0)
                {
                    std::get<1>(val).send(std::move(msg));
                }
                // and, just to be safe, reset our status!
                new (&status) completion::status(nullptr);
            }
            else
            {
                // contribute to the all_reduce with other participants
                auto cb = this->collection_proxy()
                              .callback<&completion::receive_count_>();
                auto count = make_message<count_message>(idx, status.lcount);
                this->element_proxy()
                    .contribute<&cmk::add<typename count_message::type>>(
                        std::move(count), cb, status.tag);
            }
        }

        // produce one or more events
        void produce(collection_index_t idx, std::int64_t n = 1)
        {
            message_ptr<detection_message> nil;
            this->get_status(idx, nil).lcount += n;
        }

        // consume one or more events
        void consume(collection_index_t idx, std::int64_t n = 1)
        {
            this->produce(idx, -n);
        }

    private:
        // receive the global-count from the all-reduce
        // and update the status accordingly
        void receive_count_(message_ptr<count_message>&& msg)
        {
            auto& val = msg->value();
            auto& target_idx = val.target;
            message_ptr<detection_message> nil;
            auto& status = this->get_status(target_idx, nil);
            status.complete = (val.gcount == 0);
            // this may occur if there's a hash collision during 
            // dectection when there are multiple chare arrays
            CmiAssertMsg((bool)status.msg, "received orphan count!");
            this->start_detection(std::move(status.msg));
        }

        static collective_id_t compress(collection_index_t const& idx) {
            std::hash<int> hasher;
            auto hash = (std::size_t)9;
            hash = hash * 15 + hasher(idx.pe_);
            hash = hash * 15 + hasher(idx.id_);
            return hash;
        }
    };

    completion* system_detector_(void);
}    // namespace cmk

#endif
