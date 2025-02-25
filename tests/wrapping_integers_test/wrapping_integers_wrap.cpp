#include <iostream>
#include <cstdint>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>
#include "test_should_be.h"
#include "wrapping_integers.h"
using namespace std;

int main()
{
    try
    {
        test_should_be(Wrap32::wrap(3 * (1LL << 32), Wrap32(0)), Wrap32(0));
        test_should_be(Wrap32::wrap(3 * (1LL << 32) + 17, Wrap32(15)), Wrap32(32));
        test_should_be(Wrap32::wrap(7 * (1LL << 32) - 2, Wrap32(15)), Wrap32(13));
    }
    catch (const exception &e)
    {
        cerr << e.what() << endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
