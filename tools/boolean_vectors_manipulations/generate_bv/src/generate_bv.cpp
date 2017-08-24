#include <algorithm>
#include <iostream>
#include <fstream>      // std::ifstream
#include <string.h>
#include <stdio.h>
#include "boolean_vector.hpp"
using namespace std;


long double number_of_lines(const std::string & file_name){
    long double number_of_lines = 0;
    std::string line;
    std::ifstream myfile(file_name);
    
    for (std::string line; std::getline(myfile, line); ) {
        if (line[0]!='#')
            ++number_of_lines;
    }
    myfile.close();
    return number_of_lines;
}

long double get_highest_id(const std::string & file_name){
    std::string line;
    std::ifstream myfile(file_name);
    long double highest=0;
    for (std::string line; std::getline(myfile, line); ) {
        if ( line[0]=='#'){
            continue;
        }
        std::istringstream iss(line);
        std::string token;
        long double id=-1;
        
        // a line for linker is: 0:808-89-89.000000 675-93-93.000000 or 0 3.614286 4 2 5 100.000000
        // we need to get first value (here 0)
        std::replace( line.begin(), line.end(), ':', ' ');
        //        cout<<line<<endl;
        while (std::getline(iss, token, ' '))
        {
            id=std::stold( token );
            if (id>highest) highest=id;
            break;
        }
    }
    myfile.close();
    return highest+1;
}

BooleanVector create_bv(const std::string & short_read_connector_file_name, const int threshold, const string type){
    bool linker;
    if (type=="l") {cout<<"Linker mode detected"<<endl; linker=true;}
    else if (type=="c") {cout<<"Counter mode detected"<<endl; linker=false;}
    else {
        cerr<<"type shoud be 'l' or 'c', not "<<type<<endl;
        exit(1);
    }
    // create the boolean vector
    BooleanVector bv = BooleanVector();
//    cout<<"HIGH "<<get_highest_id(short_read_connector_file_name)<<endl;
    bv.init_false(get_highest_id(short_read_connector_file_name));
    string comment("Boolean vector from linker file "+short_read_connector_file_name+" Extracted with threshold "+to_string(threshold)+"\n");
    
    // Parse the short_read_connector file
    ifstream is(short_read_connector_file_name); // open a file stream
    string line; // S string for a line to read
    for (std::string line; std::getline(is, line); ) {
        // Get comments
        if ( line[0]=='#'){
//            comment.append((line)+"\n");
            continue;
        }
        
        std::istringstream iss(line);
        std::string token;
        long double id=-1;
        int position=-1;
        if (linker) {
            /// FOR LINKER
            
            // a line for linker is: 0:808-89-89.000000 675-93-93.000000
            // we need to get first value (here 0, the id of the query read) and last float values (89.000000 and 93.000000)
            // format into 0 808 89 89.000000 675 93 93.000000
            std::replace( line.begin(), line.end(), ':', ' ');
            std::replace( line.begin(), line.end(), '-', ' ');
            //        cout<<line<<endl;
            while (std::getline(iss, token, ' '))
            {
                ++position;
                //            cout<<to_string(position)<<" "<<token<<endl;
                if ( position==0 ){id=std::stold( token ); continue;}
                if ( position%3==0 ){
                    float value=std::stof( token );
                    if ( value>threshold ){
                        
                        //                    cout<<"HOHOHO"<<to_string(position)<<" "<<token<<endl;
                        bv.set(id);
                        break;
                    }
                }
            }
        }// END FOR LINKER
        else { // FOR COUNTER
            // a line for counter is: 0 3.614286 4 2 5 100.000000
            // // we need to get first value (here 0, the id of the query read) and last float value (100.0000000)
            while (std::getline(iss, token, ' ')){
                ++position;
                //            cout<<to_string(position)<<" "<<token<<endl;
                if ( position==0 ){id=std::stold( token ); continue;}
                if ( position==5 ){
                    float value=std::stof( token );
//                    cout<<"value "<<value<<endl;
                    if ( value>threshold ) {
//                        cout<<"ID "<<id<<endl;
                        bv.set(id);
                    }
                    break;
                }
            }
        }
        
    }
    bv.set_comment(comment);
    //    if( !is.eof() ) {
    //        cerr << "There was an error during read\n";
    //    }
    is.close();
    return bv;
}

/********************************************************************************/
int main (int argc, char* argv[])
{
    if (argc != 5)
    {
        cerr << "you must provide an input text file generated by short read connector, a percentage threshold, a type of computation and an output bv URI:" << endl;
        cerr << "   1) input short read connector text file file" << endl;
        cerr << "   2) percentage threshold" << endl;
        cerr << "   3) type: 'l' or 'c' ('l' for linker, 'c' for counter)" << endl;
        cerr << "   3) output URI" << endl;
        return EXIT_FAILURE;
    }
    BooleanVector bv = create_bv(argv[1], atoi(argv[2]), argv[3]);
    bv.print(argv[4]);
    cout<<"Done, results are written in "<<argv[4]<<endl;
    return 0;
}