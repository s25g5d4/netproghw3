#include <sstream>
#include <iterator>

#include "commons.hpp"

std::vector<std::string> parse_command(std::string cmd_str)
{
    using namespace std;

    stringstream ss(cmd_str);
    return vector<string>(istream_iterator<string>(ss), istream_iterator<string>{});
}
