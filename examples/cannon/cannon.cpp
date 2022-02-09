/* charmlite demo
 *
 * author: aditya bhosale <adityapb@illinois.edu>
 *
 * based on jacobi2d implementation in charm++
 * https://github.com/UIUC-PPL/charm/tree/main/examples/charm%2B%2B/jacobi2d-2d-decomposition
 */

#include <charmlite/charmlite.hpp>

void done(cmk::message_ptr<>&&);

using index_type = std::tuple<int, int>;

struct block_message : public cmk::message
{
    bool is_a;
    int block;
    int size;
    double* data;

    block_message(bool is_a_, int block_, int size_)
      : cmk::message(cmk::message_helper_<block_message>::kind_,
            ALIGN8(sizeof(block_message)) + size_ * sizeof(double))
      , is_a(is_a_)
      , block(block_)
      , size(size_)
      , data((double*) ((char*) this + ALIGN8(sizeof(block_message))))
    {
    }

    static void pack(cmk::message_ptr<block_message>& msg)
    {
        msg->data = (double*) ((char*) msg->data - (std::uintptr_t) msg.get());
    }

    static void unpack(cmk::message_ptr<block_message>& msg)
    {
        msg->data = (double*) ((char*) msg->data + (std::uintptr_t) msg.get());
    }
};

class block : public cmk::chare<block, index_type>
{
    int block_size, nb_per_dim;
    std::size_t total_size;
    double* data;
    CthThread active;
    std::map<int, std::vector<cmk::message_ptr<block_message>>> blocks;

public:
    using construct_pack = std::tuple<int, int, bool>;
    using construct_message = cmk::data_message<construct_pack>;

    block(cmk::message_ptr<construct_message>&& msg)
    {
        auto& pack = msg->value();
        nb_per_dim = std::get<0>(pack);
        block_size = std::get<1>(pack);
        total_size = block_size * (std::size_t) block_size;
        data = (double*) aligned_alloc(
            alignof(double), total_size * sizeof(double));
        auto& randomize = std::get<2>(pack);
        if (randomize)
        {
            for (auto i = 0; i < block_size; i++)
            {
                for (auto j = 0; j < block_size; j++)
                {
                    data[i * block_size + j] = drand48();
                }
            }
        }
    }

    ~block()
    {
        free(data);
    }

    void send_data(cmk::collection_proxy<block> const& dst, bool is_a)
    {
        cmk::message_ptr<block_message> msg(
            new (sizeof(block_message) + total_size * sizeof(double))
                block_message(is_a, 0, (int) total_size));
        memcpy(msg->data, this->data, total_size * sizeof(double));
        auto& self_idx = this->index();
        auto dst_idx = is_a ?
            index_type{
                (std::get<0>(self_idx) - std::get<1>(self_idx) + nb_per_dim) %
                    nb_per_dim,
                std::get<1>(self_idx)} :
            index_type{std::get<0>(self_idx),
                (std::get<1>(self_idx) - std::get<0>(self_idx) + nb_per_dim) %
                    nb_per_dim};
        dst[dst_idx].send<&block::receive_data>(std::move(msg));
    }

    void receive_data(cmk::message_ptr<block_message>&& data)
    {
        auto& buf = this->blocks[data->block];
        buf.emplace_back(std::move(data));
        if ((buf.size() == 2) && this->active)
        {
            CthResume(this->active);
        }
    }

    void run(void)
    {
        active = CthCreate((CthVoidFn) &block::run_, this, 128 * 1024);
        CthResume(active);
    }

private:
    static void kernel_(int M, int N, int K, const double* __restrict__ A,
        const double* __restrict__ B, double* __restrict__ C)
    {
        for (int i = 0; i < M; ++i)
        {
            for (int j = 0; j < N; ++j)
            {
                double sum = 0.0;
                for (int k = 0; k < K; ++k)
                {
                    sum += A[i * K + k] * B[k * N + j];
                }
                C[N * i + j] = C[N * i + j] + sum;
            }
        }
    }

    static void run_(void* raw)
    {
        auto* self = (block*) raw;
        auto self_proxy = self->collection_proxy();
        auto& self_idx = self->index();

        for (auto block = 0; block < self->nb_per_dim; block += 1)
        {
            auto& buf = self->blocks[block];
            if (buf.size() < 2)
            {
                // go to sleep since we haven't received all values
                CthSuspend();
            }
            else
            {
                // force block_a to be at index 0
                if (buf[1]->is_a)
                {
                    std::swap(buf[0], buf[1]);
                }
                // do a quick sanity check
                CmiEnforce(buf[0]->is_a && (buf.size() == 2));
            }

            auto& block_a = buf[0];
            auto& block_b = buf[1];
            kernel_(self->block_size, self->block_size, self->block_size,
                block_a->data, block_b->data, self->data);

            if ((block + 1) < self->nb_per_dim)
            {
                // forward the block to a
                block_a->block += 1;
                self_proxy[{(std::get<0>(self_idx) + 1) % self->nb_per_dim,
                               std::get<1>(self_idx)}]
                    .send<&block::receive_data>(std::move(block_a));
                // forward the block to b
                block_b->block += 1;
                self_proxy[{std::get<0>(self_idx),
                               (std::get<1>(self_idx) + 1) % self->nb_per_dim}]
                    .send<&block::receive_data>(std::move(block_b));
            }
        }

        self->active = nullptr;
        self->element_proxy().contribute<cmk::nop<>>(
            cmk::make_message<cmk::message>(),
            cmk::callback<cmk::message>::construct<&done>(0));
    }
};

double start_time;

void done(cmk::message_ptr<>&&)
{
    CmiPrintf("main> matmul finished in %lg s\n", CmiWallTimer() - start_time);
    cmk::exit();
}

int main(int argc, char** argv)
{
    cmk::initialize(argc, argv);
    if (CmiMyNode() == 0)
    {
        auto alpha = 1.0l;
        auto np = CmiNumPes();
        decltype(np) np_per_dim = sqrt((double) np);
        auto nb_per_dim = ((np > 1) && (np_per_dim == 1)) ? 2 : np_per_dim;
        index_type array_shape = {nb_per_dim, nb_per_dim};
        auto grid_size = (argc > 1) ? atoi(argv[1]) : 128;
        CmiEnforce(grid_size % nb_per_dim == 0);
        auto block_size = grid_size / nb_per_dim;
        index_type block_shape = {block_size, block_size};
        CmiPrintf("main> matmul with %d x %d chare-array on %d pes\n",
            nb_per_dim, nb_per_dim, np);
        CmiPrintf(
            "main> each chare has %d x %d values\n", block_size, block_size);
        cmk::collection_options opts(array_shape);
        auto msg = cmk::make_message<block::construct_message>(
            nb_per_dim, block_size, true);
        auto a = cmk::collection_proxy<block>::construct(
            msg->clone<block::construct_message>(), opts);
        auto b = cmk::collection_proxy<block>::construct(
            msg->clone<block::construct_message>(), opts);
        std::get<2>(msg->value()) = false;    // do not randomize c's value
        auto c = cmk::collection_proxy<block>::construct(std::move(msg), opts);
        start_time = CmiWallTimer();
        a.broadcast<&block::send_data>(c, true);
        b.broadcast<&block::send_data>(c, false);
        c.broadcast<&block::run>();
    }
    cmk::finalize();
    return 0;
}
