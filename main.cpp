#include <iostream>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <fstream>

using namespace std;
using namespace std::filesystem;

struct Token
{
    uint16_t offset = 0;
    uint16_t length = 0;
    unsigned char next_char;
};

void flush_token_vector(vector<Token *> &token_vector, uint8_t &byte_flag, ofstream &output_file)
{
    output_file.put(byte_flag);
    byte_flag = 0;
    for (Token *token : token_vector)
    {
        if (token->offset == 0 && token->length == 0)
        {
            output_file.put(token->next_char);
        }
        else
        {
            output_file.write(reinterpret_cast<const char *>(&token->offset), sizeof(uint16_t));
            output_file.write(reinterpret_cast<const char *>(&token->length), sizeof(uint16_t));
            output_file.put(token->next_char);
        }
        delete token;
    }
    token_vector.clear();
}


int compress(vector<unsigned char> &input_file_buffer, ofstream &output_file)
{
    const uint32_t current_window_size = 1024;
    const uint32_t look_ahead_window_size = 256;
    const uint32_t input_file_size = input_file_buffer.size();

    if (input_file_size <= current_window_size)
        return 1;

    vector<Token *> token_vector;
    uint8_t byte_flag = 0;

    uint32_t current_position = current_window_size;
    int index = 8;

    while (current_position < input_file_size)
    {
        int window_start = max(0, (int)current_position - (int)current_window_size);
        int window_end = current_position;
        int lookahead_end = min((int)current_position + (int)look_ahead_window_size, (int)input_file_size);

        int best_length = 0, best_offset = 0;

        for (int i = window_start; i < window_end; ++i)
        {
            int length = 0;
            while (i + length < window_end &&
                   current_position + length < input_file_size &&
                   input_file_buffer[i + length] == input_file_buffer[current_position + length])
            {
                length++;
            }
            if (length > best_length)
            {
                best_length = length;
                best_offset = window_end - i;
            }
        }

        Token *token = new Token;
        token->length = best_length;
        token->offset = best_offset;
        token->next_char = (current_position + best_length < input_file_size)
                               ? input_file_buffer[current_position + best_length]
                               : 0;

        if (token->offset == 0 && token->length == 0)
        {
            byte_flag |= (1 << (index - 1)); 
        }

        token_vector.push_back(token);
        current_position += (best_length > 0) ? best_length + 1 : 1;

        index--;
        if (index == 0)
        {
            flush_token_vector(token_vector, byte_flag, output_file);
            index = 8;
        }


    }

    flush_token_vector(token_vector, byte_flag, output_file);


    return 0;
}

// arguments: [input_file_path] [optional_output_folder]
int main(int arg_count, char *arg_vector[])
{
    if (arg_count < 2)
    {
        cerr << "Enter arguments\n";
        return -1;
    }

    path input_file_path(arg_vector[1]);
    path output_file_path;

    if (!exists(input_file_path))
    {
        cerr << "Path does not exist\n";
        return -1;
    }

    if (arg_count < 3)
    {
        output_file_path = input_file_path.parent_path();
    }
    else
    {
        output_file_path = arg_vector[2];
    }

    string input_file_name = input_file_path.filename().string();
    string output_file_name = input_file_name.substr(0, input_file_name.find_last_of('.'));
    output_file_path /= output_file_name;

    uint32_t input_file_size = file_size(input_file_path);

    ifstream input_file(input_file_path, ios::binary);
    ofstream output_file(output_file_path, ios::binary);

    vector<unsigned char> input_file_buffer(
        (istreambuf_iterator<char>(input_file)), istreambuf_iterator<char>());

    input_file.close(); 

    int com = compress(input_file_buffer, output_file);

    uint32_t output_file_size = file_size(output_file_path);

    output_file.close(); 

    if (com)
    {
        cerr << "Error in compression!" << endl;
        return 0;
    }

    cout << "Compression successful\n";
    cout << "File Name: " << input_file_name << " → " << output_file_name << endl;
    cout << "Original Size: " << input_file_size << " bytes\n";
    cout << "Compressed Size: " << output_file_size << " bytes\n";
    return 0;
}
