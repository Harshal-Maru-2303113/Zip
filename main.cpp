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

void flush_token_vector(vector<Token *> &token_vector, uint8_t &byte_flag, int valid_bits, ofstream &output_file)
{
    if (token_vector.empty())
        return;

    output_file.put(byte_flag);

    for (int i = 0; i < valid_bits; ++i)
    {
        Token *token = token_vector[i];
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
    byte_flag = 0;
}

int compress(ifstream &input_file, ofstream &output_file)
{
    const uint32_t current_window_size = 1024;
    const uint32_t look_ahead_window_size = 256;

    vector<unsigned char> input_file_buffer((istreambuf_iterator<char>(input_file)), {});
    const size_t input_size = input_file_buffer.size();

    if (input_size == 0){
        return 1;
    }

    vector<Token *> token_vector;
    uint8_t byte_flag = 0;
    int bit_position = 0; 
    size_t current_position = 0;

    while (current_position < input_size)
    {
        int window_start = max(0, (int)current_position - (int)current_window_size);
        int window_end = current_position;
        int best_length = 0, best_offset = 0;

        for (int i = window_start; i < window_end; ++i)
        {
            int length = 0;
            while (i + length < window_end &&
                   current_position + length < input_size &&
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

        if (current_position < current_window_size || best_length == 0 || best_offset == 0)
        {
            token->offset = 0;
            token->length = 0;
            token->next_char = input_file_buffer[current_position];
            byte_flag &= ~(1 << bit_position); 
            current_position += 1;
        }
        else
        {
            token->offset = best_offset;
            token->length = best_length;
            token->next_char = (current_position + best_length < input_size)
                                   ? input_file_buffer[current_position + best_length]
                                   : 0;
            byte_flag |= (1 << bit_position); 
            current_position += best_length + 1;
        }

        token_vector.push_back(token);
        bit_position++;

        if (bit_position == 8)
        {
            flush_token_vector(token_vector, byte_flag, 8, output_file);
            bit_position = 0;
        }
    }

    if (!token_vector.empty())
    {
        flush_token_vector(token_vector, byte_flag, bit_position, output_file);
    }

    return 0;
}

int decompress(ifstream &input_file, ofstream &output_file)
{
    if (!input_file.is_open() || !output_file.is_open())
    {
        cerr << "File open error.\n";
        return -1;
    }

    vector<unsigned char> decompressed_data;

    while (true)
    {
        uint8_t flag_byte;
        input_file.read(reinterpret_cast<char *>(&flag_byte), 1);
        // check the flag
        if (input_file.eof() || input_file.gcount() != 1){
            break;
        }

        for (int i = 0; i < 8; ++i)
        {
            // Check end of the file
            if (input_file.peek() == EOF){
                break;
            }

            if ((flag_byte & (1 << i)) != 0)
            {
                // Token
                uint16_t offset = 0, length = 0;
                unsigned char next_char = 0;

                input_file.read(reinterpret_cast<char *>(&offset), sizeof(offset));
                if (input_file.gcount() != sizeof(offset))
                {
                    if (input_file.eof()){
                        break; 
                    }
                    cerr << "Unexpected EOF while reading offset.\n";
                    return -2;
                }

                input_file.read(reinterpret_cast<char *>(&length), sizeof(length));
                if (input_file.gcount() != sizeof(length))
                {
                    if (input_file.eof()){
                        break; 
                    }
                    cerr << "Unexpected EOF while reading length.\n";
                    return -2;
                }

                input_file.read(reinterpret_cast<char *>(&next_char), sizeof(next_char));
                if (input_file.gcount() != sizeof(next_char))
                {
                    if (input_file.eof()){
                        break; 
                    }
                    cerr << "Unexpected EOF while reading next_char.\n";
                    return -2;
                }

                if (offset == 0 || decompressed_data.size() < offset)
                {
                    cerr << "Invalid offset " << offset << " at position " << decompressed_data.size() << '\n';
                    return -3;
                }

                size_t start = decompressed_data.size() - offset;
                for (int j = 0; j < length; ++j)
                {
                    decompressed_data.push_back(decompressed_data[start + j]);
                }
                decompressed_data.push_back(next_char);
            }
            else
            {
                // Literal
                unsigned char literal = 0;
                input_file.read(reinterpret_cast<char *>(&literal), sizeof(literal));
                if (input_file.gcount() != sizeof(literal))
                {
                    if (input_file.eof())
                        break; // End of file reached
                    cerr << "Unexpected EOF in literal.\n";
                    return -4;
                }
                decompressed_data.push_back(literal);
            }
        }
    }

    output_file.write(reinterpret_cast<char *>(decompressed_data.data()), decompressed_data.size());
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        cerr << "Usage:\n"
             << "  " << argv[0] << " -c input_file [output_dir]\n"
             << "  " << argv[0] << " -d input_file [output_dir]\n";
        return -1;
    }

    string mode = argv[1];
    path input_file_path(argv[2]);

    if (!exists(input_file_path))
    {
        cerr << "Error: Input file does not exist\n";
        return -1;
    }

    path output_dir = (argc >= 4) ? argv[3] : input_file_path.parent_path();
    create_directories(output_dir); 

    string input_file_name = input_file_path.filename().string();
    string stem = input_file_path.stem().string(); 
    string extension = input_file_path.extension().string();

    path output_file_path;
    if (mode == "-c")
    {
        output_file_path = output_dir / ("compressed_" + stem + extension);
    }
    else if (mode == "-d")
    {
        output_file_path = output_dir / ("decompressed_" + stem + extension);
    }
    else
    {
        cerr << "Unknown mode: " << mode << ". Use -c or -d.\n";
        return -1;
    }

    uint32_t input_file_size = file_size(input_file_path);
    uint32_t output_file_size = file_size(output_file_path);

    ifstream input_file(input_file_path, ios::binary);
    ofstream output_file(output_file_path, ios::binary);

    int result = 0;
    if (mode == "-c")
    {
        result = compress(input_file, output_file);
    }
    else
    {
        result = decompress(input_file, output_file);
    }

    input_file.close();
    output_file.close();

    if (result != 0)
    {
        cerr << (mode == "-c" ? "Compression" : "Decompression") << " failed.\n";
        return result;
    }

    cout << (mode == "-c" ? "Compression" : "Decompression") << " successful\n";
    cout << "Input:  " << input_file_name << '\n';
    cout << "Output: " << output_file_path.filename().string() << '\n';
    cout << "Size:   " << file_size(input_file_path) << " -> " << file_size(output_file_path) << " bytes\n";

    double ratio = static_cast<double>(input_file_size) / output_file_size;
    double saved_percent = 100.0 * (1.0 - static_cast<double>(output_file_size) / input_file_size);

    cout << fixed << setprecision(2);
    cout << "Compression Ratio: " << ratio << "x\n";
    cout << "Space Saved: " << saved_percent << "%\n";

    return 0;
}