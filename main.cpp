#include <iostream>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <fstream>

using namespace std;
using namespace std::filesystem;

// arguments: [input_file_path]
int main(int arg_count, char *arg_vector[])
{
    if (arg_count < 2)
    {
        cerr << "Enter arguments\n";
        return -1;
    }

    path input_file_path(arg_vector[1]);

    if (!exists(input_file_path))
    {
        cerr << "Path does not exists\n";
        return -1;
    }

    string input_file_name = input_file_path.filename().string();
    uint32_t input_file_size = file_size(input_file_path);

    ifstream input_file(input_file_path, ios::binary);

    vector<unsigned char> input_file_buffer((istreambuf_iterator<char>(input_file)), istreambuf_iterator<char>());

    cout << "\n";
   
    cout << "File Name: " << input_file_name << endl;
    cout << "File Size: " << input_file_size << endl;
    return 0;
}