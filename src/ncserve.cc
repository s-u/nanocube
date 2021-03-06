// #include <stdio.h>
// #include <stdlib.h>

#include <ext/stdio_filebuf.h>

#include <unistd.h>    /* for fork */
#include <sys/types.h> /* for pid_t */
#include <sys/wait.h>  /* for wait */

#include <algorithm>
#include <fstream>
#include <sstream>
#include <iostream>

#include <DumpFile.hh>


void message() {

    std::cout << "Usage: frontend <number>" << std::endl;

}

//
// Here is the entity responsible to translating dump file
// field type names into a meaningful nanocube schema
//
struct NanoCubeSchema {

    NanoCubeSchema(dumpfile::DumpFileDescription &dump_file_description):
        dump_file_description(dump_file_description)
    {
        std::stringstream ss_dimensions_spec;
        std::stringstream ss_variables_spec;
        for (dumpfile::Field *field: dump_file_description.fields) {
            std::string field_type_name = field->field_type.name;
            std::cout << field_type_name << std::endl;
            if (field_type_name.find("nc_dim_cat_") == 0) {
                auto pos = field_type_name.begin() + field_type_name.rfind('_');
                int num_bytes = std::stoi(std::string(pos+1,field_type_name.end()));
                std::cout << "categorical dimension with " << num_bytes << " bytes" << std::endl;
                ss_dimensions_spec << "_c" << num_bytes;
            }
            else if (field_type_name.find("nc_dim_quadtree_") == 0) {
                auto pos = field_type_name.begin() + field_type_name.rfind('_');
                int num_levels = std::stoi(std::string(pos+1,field_type_name.end()));
                std::cout << "quadtree dimension with " << num_levels << " levels" << std::endl;
                ss_dimensions_spec << "_q" << num_levels;
            }
            else if (field_type_name.find("nc_dim_time_") == 0) {
                auto pos = field_type_name.begin() + field_type_name.rfind('_');
                int num_bytes = std::stoi(std::string(pos+1,field_type_name.end()));
                std::cout << "time dimension with " << num_bytes << " bytes" << std::endl;
                ss_variables_spec << "_u" << num_bytes;
            }
            else if (field_type_name.find("nc_var_uint_") == 0) {
                auto pos = field_type_name.begin() + field_type_name.rfind('_');
                int num_bytes = std::stoi(std::string(pos+1,field_type_name.end()));
                std::cout << "time dimension with " << num_bytes << " bytes" << std::endl;
                ss_variables_spec << "_u" << num_bytes;
            }

            this->dimensions_spec         = ss_dimensions_spec.str();
            this->time_and_variables_spec = ss_variables_spec.str();
        }
    }

    std::string dimensions_spec;

    std::string time_and_variables_spec;

    dumpfile::DumpFileDescription &dump_file_description;

};

//
// d stands for dimension
//
//    cX = categorical, X = num bytes
//    qX = quadtree,    X = num levels
//    tX = time,        X = num bytes
//
// v stands for variable
//
//    uX = unsigned int of X bytes
//

int main(int argc, char **argv)
{

    // get process id
    // pid_t my_pid = getpid();
    // std::cout << argv[0] << " process id: " << my_pid << std::endl;


    // read input file description
    dumpfile::DumpFileDescription input_file_description;
    std::cin >> input_file_description;

    // read schema
    NanoCubeSchema nc_schema(input_file_description);
    std::cout << "Dimensions: " << nc_schema.dimensions_spec << std::endl;
    std::cout << "Variables:  " << nc_schema.time_and_variables_spec << std::endl;

    //
    // TODO: run some tests
    //
    //    (1) is there a single time column
    //    (2) are all field types starting with nc_ prefix
    //

    // pipe
    int pipe_one[2];
    int &pipe_read_file_descriptor  = pipe_one[0];
    int &pipe_write_file_descriptor = pipe_one[1];

    // create pipe
    if(pipe(pipe_one) == -1)
    {
        std::cout << "Couldn't create pipe!" << std::endl;
        // perror("ERROR");
        exit(127);
    }

    // Spawn a child to run the program.
    pid_t pid = fork();
    if (pid == 0) { /* child process */

        /* Close unused pipe */
        close(pipe_write_file_descriptor);

        /* both file descriptors refer to pipe_read_file_descriptor */
        /* std::cin becomes the pipe read channel */
        dup2(pipe_read_file_descriptor, STDIN_FILENO);

        // exec new process
        std::string program_name;
        {
            std::stringstream ss;

            const char* binaries_path_ptr = std::getenv("NANOCUBE_BIN");
            if (binaries_path_ptr) {
                std::string binaries_path(binaries_path_ptr);
                if (binaries_path.size() > 0 && binaries_path.back() != '/') {
                    binaries_path = binaries_path + "/";
                }
                ss << binaries_path;
            }
            else {
                ss << "./";
            }

            ss << "nc" << nc_schema.dimensions_spec << nc_schema.time_and_variables_spec;
            program_name = ss.str();
        }

        // child process will be replaced by this other process
        // could we do thi in the main process? maybe
        //execlp(program_name.c_str(), program_name.c_str(), NULL);
        execv(program_name.c_str(), argv);

        // failed to execute program
        std::cout << "Could not find program: " << program_name << std::endl;

        exit(127); /* only if execv fails */

    }
    else {

        /* Close unused pipe */
        // close(pipe_read_file_descriptor);

        /* Attach pipe write file to stdout */
        // dup2(pipe_write_file_descriptor, STDOUT_FILENO);
        // std::ostream &os = std::cout;

        // int posix_handle = ::_fileno(::fopen("test.txt", "r"));
        // output stream
        __gnu_cxx::stdio_filebuf<char> filebuf(pipe_write_file_descriptor, std::ios::out);
        std::ostream os(&filebuf); // 1

        // std::ofdstream pipe_output_stream(pipe_write_file_descriptor);

        // restream the schema
        os << input_file_description;
        // pipe_output_stream << input_file_description;

        // signal that data is going to come
        os << std::endl;
        // pipe_output_stream << std::endl;

        // write everything coming from stdin to child process
        const int BUFFER_SIZE = 4095;
        char buffer[BUFFER_SIZE + 1];
        while (1) {
            std::cin.read(buffer,BUFFER_SIZE);
            if (!std::cin) {
                int gcount = std::cin.gcount();
                if (gcount > 0) {
                    // std::string st(&buffer[0], &buffer[gcount]);
                    // std::replace( st.begin(), st.end(), '\n', 'L');
                    // std::out << "server writes > " << st << std::endl;
                    os.write(buffer, gcount);
                }
                break;
            }
            else {
                os.write(buffer, BUFFER_SIZE);
                // std::string st(&buffer[0], &buffer[BUFFER_SIZE]);
                // std::replace( st.begin(), st.end(), '\n', 'L');
                // std::cerr << "server writes > " << st << std::endl;
            }
        }

        // clear
        os.flush();

        //
        // std::cerr << "Waiting for child process to finish" << std::endl;

        // done writing on the pipe
        close(pipe_write_file_descriptor);

        //
        waitpid(pid,0,0); /* wait for child to exit */

    }

    return 0;

}























#if 0
    int number;

    try
    {
        if (argc < 2) throw std::exception();
        number = std::stoi(argv[1]);
    }
    catch(std::exception &e)
    {
        message();
        exit(0);
    }

    /*Spawn a child to run the program.*/
    // pid_t pid=fork();
    // if (pid==0) { /* child process */

    std::string program_name;
    {
        std::stringstream ss;
        ss << "/Users/lauro/tests/fork-exec/backend_" << number;
        program_name = ss.str();
    }

    static char *other_argv[] = {"backend", NULL};

    // child process will be replaced by this other process
    // could we do thi in the main process? maybe
    execv(program_name.c_str(), other_argv);

    // failed to execute program
    std::cout << "Could not find program: " << program_name << std::endl;

    exit(127); /* only if execv fails */

    // }
    // else { /* pid!=0; parent process */
    //     waitpid(pid,0,0); /* wait for child to exit */
    // }
    return 0;

#endif


/*<@>
<@> ******** Program output: ********
<@> Foo is my name.
<@> */

