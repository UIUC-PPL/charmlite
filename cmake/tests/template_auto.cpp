template <auto Val>
void do_nohing()
{
}

int main(int argc, char* argv[])
{
    do_nohing<42>();
    do_nohing<'a'>();
}
