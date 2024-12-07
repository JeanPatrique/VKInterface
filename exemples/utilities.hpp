#ifndef LIBS_GAMEUTILS_UTILITIES_HEADER
#define LIBS_GAMEUTILS_UTILITIES_HEADER

#include <iostream>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstring>
#include <chrono>

//extern "C" {
namespace GU
{

    /**@brief Duplicate a null-terminated string.
     * @warning The returned string is dynamically allocated and thus must be delete[].
     */
    char* duplicateString(const char*); // duplicate a c-string.

    /**@brief Read a file in binary and return it's content.*/
    typedef std::vector<char> Bytecode;
    Bytecode readFile(const std::filesystem::path&);

    /**@brief This function is a convenien way to record the frame rate.
     *        By calling this function with nullptr, the time difference between the last call is stored.
     *        Then the mean of the differences can be return by setting a value to frameRate.
     * @param frameRate if nullptr, the time difference is stored. 
     *                  if not, the mean of the time differencies are returned.
     * */
    void countFrameRate(double *frameRate=nullptr);


} //namespace GU
//} //extern C
//#endif//LIBS_GAMEUTILS_UTILITIES_HEADER

// Cpp file :

//extern "C" {
namespace GU
{

    // Duplicate a c-string, which is pretty usefull when you don't know the life duration of a string.
	char* duplicateString(const char* str)
	{
		const size_t len = std::strlen(str);
		char *dest = new char[len+1];
		std::strcpy(dest, str);
		return dest;
	}


    Bytecode readFile(const std::filesystem::path& location)
    {
        std::vector<char> buffer;

        std::ifstream inFile(location, std::ios::ate | std::ios::binary);
        if (not inFile.is_open())
            throw std::runtime_error(std::string("Can't open file at : ")+(location).string());

        buffer.resize(inFile.tellg());
        inFile.seekg(0);
        inFile.read(buffer.data(), buffer.size());

        inFile.close();
        return buffer;
    }

    void countFrameRate(double *frameRate)
    {
        static std::chrono::duration<double, std::micro> deltaMean;
        static std::chrono::time_point<std::chrono::high_resolution_clock> t0 = std::chrono::high_resolution_clock::now();
        static uint32_t meanSize=0;

        std::chrono::time_point<std::chrono::high_resolution_clock> t1 = std::chrono::high_resolution_clock::now();

        deltaMean += t1-t0;
        t0 = t1;
        meanSize++;

        if (frameRate!=nullptr)
        {
            deltaMean /= meanSize;
            *frameRate = (1/deltaMean.count()) * 1'000'000;
            meanSize = 0;
            deltaMean = deltaMean.zero();
        }
    }


}//namespace GU
//}/*extern "C"*/

#endif
