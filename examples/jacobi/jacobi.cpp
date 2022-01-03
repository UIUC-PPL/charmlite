/* charmlite demo
 *
 * author: aditya bhosale <adityapb@illinois.edu>
 */

#include <cstring>
#include <charmlite.hpp>

#define LEFT 0
#define RIGHT 1
#define TOP 2
#define BOTTOM 3

#define THRESHOLD 0.0004
#define MAX_ITER 10000
#define MAX_CHARE 10

class index2d
{
public:
    int x, y;

    index2d(int x_=0, int y_=0) :
        x(x_), y(y_)
    {
    }

    void operator+=(const index2d& idx)
    {
        x += idx.x;
        y += idx.y;
    }

    bool operator==(const index2d& idx)
    {
        return idx.x == x && idx.y == y;
    }

    bool operator!=(const index2d& idx)
    {
        return idx.x != x || idx.y != y;
    }
};

template <>
struct cmk::index_view<index2d>
{
    static index2d decode(chare_index_t& idx)
    {
        // reinterpret the passed index as index2d
        return index2d(idx / MAX_CHARE, idx % MAX_CHARE);
    }

    static const index2d decode(const chare_index_t& idx)
    {
        return index2d(idx / MAX_CHARE, idx % MAX_CHARE);
    }

    static chare_index_t encode(const index2d& idx)
    {
        return idx.x * MAX_CHARE + idx.y;
    }
};

struct ghost_message : public cmk::message
{
    int direction;
    int size;
    char* data;

    ghost_message(int direction_, int size_)
        : cmk::message(cmk::message_helper_<ghost_message>::kind_,
                sizeof(message) + 2 * sizeof(int) + sizeof(char*) + size_ * sizeof(double))
          , direction(direction_)
          , size(size_)
          , data((char*) this + 2 * sizeof(int) + sizeof(char*) + sizeof(message))
    {
    }

    static void pack(cmk::message_ptr<ghost_message>& msg)
    {
        msg->data = msg->data - (std::uintptr_t) msg.get();
    }

    static void unpack(cmk::message_ptr<ghost_message>& msg)
    {
        msg->data = msg->data + (std::uintptr_t) msg.get();
    }
};

struct completion_message : public cmk::plain_message<completion_message>
{
    bool converged;

    completion_message(bool converged_) 
        : converged(converged_)
    {
    }
};

struct setup_message : public cmk::plain_message<setup_message>
{
    int array_dim_x;
    int array_dim_y;
    int block_dim_x;
    int block_dim_y;
    int num_chare_x;
    int num_chare_y;

    int maxiterations;

    setup_message(int array_dim_x_, int array_dim_y_, 
            int block_dim_x_, int block_dim_y_, 
            int num_chare_x_, int num_chare_y_,
            int maxiterations_)
        : array_dim_x(array_dim_x_)
          , array_dim_y(array_dim_y_)
          , block_dim_x(block_dim_x_)
          , block_dim_y(block_dim_y_)
          , num_chare_x(num_chare_x_)
          , num_chare_y(num_chare_y_)
          , maxiterations(maxiterations_)
    {
    }
};

template <typename T = cmk::message>
cmk::message_ptr<T> logical_and(cmk::message_ptr<T>&& lhs, cmk::message_ptr<T>&& rhs)
{
    lhs->converged &= rhs->converged;
    return std::move(lhs);
}

static_assert(cmk::is_packable<ghost_message>::value,
    "expected ghost message to be packable");

class jacobi : public cmk::chare<jacobi, index2d>
{
    using array2d = std::vector<std::vector<double>>;
    using array1d = std::vector<double>;

public:
    array2d temperature;
    array2d new_temperature;
    int iterations;
    int neighbors;
    int received_ghosts;
    int istart, ifinish, jstart, jfinish;
    double max_error;
    bool left_bound, right_bound, top_bound, bottom_bound;
    int converged;

    int array_dim_x;
    int array_dim_y;
    int block_dim_x;
    int block_dim_y;

    int num_chare_x;
    int num_chare_y;

    int maxiterations;

    index2d this_index;

    jacobi(cmk::message_ptr<setup_message>&& msg) 
        : iterations(0), neighbors(0), max_error(0), converged(0)
          , array_dim_x(msg->array_dim_x) 
          , array_dim_y(msg->array_dim_x) 
          , block_dim_x(msg->block_dim_x) 
          , block_dim_y(msg->block_dim_y) 
          , num_chare_x(msg->num_chare_x) 
          , num_chare_y(msg->num_chare_y) 
          , maxiterations(msg->maxiterations) 
    {
        this_index = this->index();

        CmiPrintf("(%i, %i) on pe %i\n", this_index.x, this_index.y, CmiMyPe());

        cmk::message::free(msg);

        temperature = array2d(block_dim_x + 2, array1d(block_dim_y + 2, 0.0));
        new_temperature = array2d(block_dim_x + 2, array1d(block_dim_y + 2, 0.0));

        left_bound = right_bound = top_bound = bottom_bound = false;
        istart = jstart = 1;
        ifinish = block_dim_x + 1;
        jfinish = block_dim_y + 1;

        if(this_index.x == 0)
        {
            left_bound = true;
            istart++;
        }
        else
            neighbors++;

        if(this_index.x == num_chare_x - 1)
        {
            right_bound = true;
            ifinish--;
        }
        else
            neighbors++;

        if(this_index.y == 0)
        {
            top_bound = true;
            jstart++;
        }
        else
            neighbors++;

        if(this_index.y == num_chare_y - 1)
        {
            bottom_bound = true;
            jfinish--;
        }
        else
            neighbors++;

        constraint_bc();
        begin_iteration();
    }

    // Send ghost faces to the six neighbors
    void begin_iteration() 
    {
        iterations++;

        auto this_proxy = this->collection_proxy();

        double* ghost_data;
        int msg_size_x = sizeof(cmk::message) + 2 * sizeof(int) + 
            sizeof(char*) + block_dim_x * sizeof(double);
        int msg_size_y = sizeof(cmk::message) + 2 * sizeof(int) + 
            sizeof(char*) + block_dim_y * sizeof(double);

        if(!left_bound)
        {
            if(iterations == 1 && this_index.x == 0 && this_index.y == 0)
                CmiPrintf("Sent from (0, 0) to right\n");
            cmk::message_ptr<ghost_message> msg(
                new (msg_size_y) ghost_message(RIGHT, block_dim_y));
            ghost_data = (double*) msg->data;
            for(int j = 0; j < block_dim_y; ++j) 
                ghost_data[j] = temperature[1][j+1];
            this_proxy[index2d(this_index.x - 1, this_index.y)].send<
                ghost_message, &jacobi::receive_ghosts>(std::move(msg));
        }
        if(!right_bound)
        {
            if(iterations == 1 && this_index.x == 0 && this_index.y == 0)
                CmiPrintf("Sent from (0, 0) to left\n");
            cmk::message_ptr<ghost_message> msg(
                new (msg_size_y) ghost_message(LEFT, block_dim_y));
            ghost_data = (double*) msg->data;
            for(int j = 0; j < block_dim_y; ++j) 
                ghost_data[j] = temperature[block_dim_x][j+1];
            this_proxy[index2d(this_index.x + 1, this_index.y)].send<
                ghost_message, &jacobi::receive_ghosts>(std::move(msg));
        }
        if(!top_bound)
        {
            if(iterations == 1 && this_index.x == 0 && this_index.y == 0)
                CmiPrintf("Sent from (0, 0) to bottom\n");
            cmk::message_ptr<ghost_message> msg(
                new (msg_size_x) ghost_message(BOTTOM, block_dim_x));
            ghost_data = (double*) msg->data;
            for(int i = 0; i < block_dim_x; ++i) 
                ghost_data[i] = temperature[i+1][1];
            this_proxy[index2d(this_index.x, this_index.y - 1)].send<
                ghost_message, &jacobi::receive_ghosts>(std::move(msg));
        }
        if(!bottom_bound)
        {
            if(iterations == 1 && this_index.x == 0 && this_index.y == 0)
                CmiPrintf("Sent from (0, 0) to top\n");
            cmk::message_ptr<ghost_message> msg(
                new (msg_size_x) ghost_message(TOP, block_dim_x));
            ghost_data = (double*) msg->data;
            for(int i = 0; i < block_dim_x; ++i)
                ghost_data[i] = temperature[i+1][block_dim_y];
            this_proxy[index2d(this_index.x, this_index.y + 1)].send<
                ghost_message, &jacobi::receive_ghosts>(std::move(msg));
        }

        //dump_matrix(temperature);
    }

    void receive_ghosts(cmk::message_ptr<ghost_message>&& msg)
    {
        if(iterations == 1)
            CmiPrintf("Received from %i at (%i, %i), pe = %i\n", 
                msg->direction, this_index.x, this_index.y, CmiMyPe());
        process_ghosts(msg->direction, msg->size, (double*) msg->data);
        if(++received_ghosts == neighbors)
        {
            // received all neighbors, start next iteration
            received_ghosts = 0;
            compute();
        }
    }

    void process_ghosts(int dir, int size, double* gh) 
    {
        switch(dir) 
        {
        case LEFT:
            for(int j = 0; j < size; ++j) 
                temperature[0][j + 1] = gh[j];
            break;
        case RIGHT:
            for(int j = 0; j < size; ++j) 
                temperature[block_dim_x + 1][j + 1] = gh[j];
            break;
        case TOP:
            for(int i = 0; i < size; ++i)
                temperature[i + 1][0] = gh[i];
            break;
        case BOTTOM:
            for(int i = 0; i < size; ++i)
                temperature[i + 1][block_dim_y + 1] = gh[i];
            break;
        default:
            CmiAbort("ERROR\n");
        }
    }

    void compute() 
    {
        auto this_proxy = this->collection_proxy();

        double temperature_ij = 0.;
        double difference = 0.;

        max_error = 0.;
        // When all neighbor values have been received, we update our values and proceed
        for(int i = istart; i < ifinish; ++i) 
        {
            for(int j = jstart; j < jfinish; ++j) 
            {
                temperature_ij = (temperature[i][j] + temperature[i-1][j] + 
                        temperature[i+1][j] + temperature[i][j-1] + 
                        temperature[i][j+1]) * 0.2;

                // update relative error
                difference = temperature_ij - temperature[i][j];
                // fix sign without fabs overhead
                if(difference < 0) difference *= -1.0; 
                max_error = (max_error > difference) ? max_error : difference;
                new_temperature[i][j] = temperature_ij;
            }
        }

        temperature.swap(new_temperature);

        bool converged = (max_error <= THRESHOLD);
        auto msg = cmk::make_message<completion_message>(converged);

        //auto cb = cmk::callback<cmk::message>::construct<jacobi::check_completion>(cmk::all_pes);
        auto cb = this_proxy.callback<completion_message, &jacobi::check_completion>();
        this->element_proxy().contribute<completion_message, 
            logical_and<completion_message>>(std::move(msg), cb);
        //CmiPrintf("Done compute\n");
        //dump_matrix(temperature);
    }

    void check_completion(cmk::message_ptr<completion_message>&& msg)
    {
        if(iterations == maxiterations || msg->converged)
        {
            CmiPrintf("Done!\n");
            dump_matrix(temperature);
            cmk::exit();
        }
        else
            begin_iteration();
    }

    // Enforce some boundary conditions
    void constraint_bc()
    {
        if(top_bound)
            for(int i = 0; i < block_dim_x + 2; ++i)
            {
                temperature[i][1] = 1.;
                new_temperature[i][1] = 1.;
            }

        if(left_bound)
            for(int j = 0; j < block_dim_y + 2; ++j)
            {
                temperature[1][j] = 1.;
                new_temperature[1][j] = 1.;
            }

        if(bottom_bound)
            for(int i = 0; i < block_dim_x + 2; ++i)
            {
                temperature[i][block_dim_y] = 1.;
                new_temperature[i][block_dim_y] = 1.;
            }

        if(right_bound)
            for(int j = 0; j < block_dim_y + 2; ++j)
            {
                temperature[block_dim_x][j] = 1.;
                new_temperature[block_dim_x][j] = 1.;
            }
    }

    // for debugging
    void dump_matrix(array2d const& matrix)
    {
        CmiPrintf("\n\n[%d,%d] iter = %i\n",this_index.x, this_index.y, iterations);
        for(int i = 0; i < block_dim_x + 2; ++i)
        {
            for(int j = 0; j < block_dim_y + 2; ++j)
                CmiPrintf("%0.3lf ",matrix[i][j]);
            CmiPrintf("\n");
        }
    }
};

int main(int argc, char** argv)
{
    cmk::initialize(argc, argv);
    if (CmiMyNode() == 0)
    {
        int array_dim_x, array_dim_y, block_dim_x, block_dim_y;

        if((argc < 3) || (argc > 6)) 
        {
            CmiPrintf("%s [array_size] [block_size]\n", argv[0]);
            CmiPrintf("OR %s [array_size] [block_size] maxiterations\n", argv[0]);
            CmiPrintf("OR %s [array_size_X] [array_size_Y] [block_size_X] [block_size_Y] \n", argv[0]);
            CmiPrintf("OR %s [array_size_X] [array_size_Y] [block_size_X] [block_size_Y] maxiterations\n", argv[0]);
            CmiAbort("Abort");
        }

        if(argc <= 4) 
        {
            array_dim_x = array_dim_y = atoi(argv[1]);
            block_dim_x = block_dim_y = atoi(argv[2]);
        }
        else if (argc >= 5) 
        {
            array_dim_x = atoi(argv[1]);
            array_dim_y = atoi(argv[2]);
            block_dim_x = atoi(argv[3]); 
            block_dim_y = atoi(argv[4]); 
        }
    
        int maxiterations = MAX_ITER;
        if(argc == 4)
            maxiterations = atoi(argv[3]); 
        if(argc == 6)
            maxiterations = atoi(argv[5]); 

        if (array_dim_x < block_dim_x || array_dim_x % block_dim_x != 0)
            CmiAbort("array_size_x %% block_size_x != 0!");
        if (array_dim_y < block_dim_y || array_dim_y % block_dim_y != 0)
            CmiAbort("array_size_y %% block_size_y != 0!");

        int num_chare_x = array_dim_x / block_dim_x;
        int num_chare_y = array_dim_y / block_dim_y;

        num_chare_x = num_chare_x > MAX_CHARE ? MAX_CHARE : num_chare_x;
        num_chare_y = num_chare_y > MAX_CHARE ? MAX_CHARE : num_chare_y;

        block_dim_x = array_dim_x / num_chare_x;
        block_dim_y = array_dim_y / num_chare_y;

        CmiPrintf("Running Jacobi on %d processors with (%d, %d) chares\n", 
                CmiNumPes(), num_chare_x, num_chare_y);
        CmiPrintf("Array Dimensions: %d %d\n", array_dim_x, array_dim_y);
        CmiPrintf("Block Dimensions: %d %d\n", block_dim_x, block_dim_y);
        CmiPrintf("Max iterations %d\n", maxiterations);
        CmiPrintf("Threshold %.10g\n", THRESHOLD);

        auto array = cmk::collection_proxy<jacobi>::construct();

        for(auto i = 0; i < num_chare_x; i++)
            for(auto j = 0; j < num_chare_y; j++)
                array[index2d(i, j)].insert(cmk::make_message<setup_message>(
                            array_dim_x, array_dim_y, block_dim_x, block_dim_y, 
                            num_chare_x, num_chare_y, maxiterations));

        CmiPrintf("Done inserting\n");
        array.done_inserting();
    }
    cmk::finalize();
    return 0;
}
