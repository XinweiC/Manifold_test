//simulation_stop = 700000000000L; //
simulation_stop = 100000000L; //

network_clock_frequency = 3000000000L; // 3GHz

distributed_clock_frequency = [3e9, 3e9, 3e9, 3e9,
                               3e9, 3e9, 3e9, 3e9,
                               3e9, 3e9, 3e9, 3e9,
                               3e9, 3e9, 3e9, 3e9];

network:
{
    topology = "TORUS6P";
    x_dimension = 4;
    y_dimension = 4;
    num_vcs = 4;
    credits = 32;
    link_width = 512;

    ni_up_credits = 64; //credits for network interface sending to terminal
    ni_up_buffer = 32; //network interface's output buffer (to terminal) size

    coh_msg_type = 123; //message types
    mem_msg_type = 456;
    credit_msg_type = 789;
};

processor:
{
    node_idx = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15];
};

llp_cache:
{
    name = "L1";
    type = "DATA";
    size = 0x4000; //16K
    assoc = 8;
    block_size = 64;
    hit_time = 1;
    lookup_time = 1;
    replacement_policy = "LRU";
    mshr_size = 32;

    downstream_credits = 32; //credits for sending to network
};

lls_cache:
{
    name = "L2";
    type = "DATA";
    size = 0x200000; //2MB
    assoc = 64;
    block_size = 64;
    hit_time = 24;
    lookup_time = 24;
    replacement_policy = "LRU";
    mshr_size = 128;

    downstream_credits = 128; //credits for sending to network
};

mc: //memory controller
{
    downstream_credits = 128; //credits for sending to network
    node_idx = [0, 3, 12, 15];
};
