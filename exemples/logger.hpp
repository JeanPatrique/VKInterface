#ifndef LIBS_GAMEUTILS_LOGGER_HEADER
#define LIBS_GAMEUTILS_LOGGER_HEADER

#include <filesystem>
#include <iostream> // cerr is used to display an error message if logger fail.
#include <fstream>
#include <mutex> // Since any gameutils componant must be thread-safe.
#include <atomic>
#include <string>
#include <memory>

#define LOGGER_MAX_LOGS_ENTRIES_PER_FILE 2000 // ~= nb line per file.

//extern "C" {
namespace GU
{
    /**@brief Classify the log entry. FATAL_ERROR guaranty the entry to be logged (force a flush to the log file).
     *  exemple :
     *      logFatalError("This entry will be in the log file.");
     *      int *p=nullptr;
     *      *p=10;
     *      
     *      $ cat logfile
     *      This entry will be in the log file.
     */
    enum struct logLevel : uint8_t
    {
        VERBOSE,
        INFO,
        WARNING,
        ERROR,
        FATAL_ERROR, // Use fatal error to notify that the program will crash.
    };
     // GU::Logger will be in the main thread.
     // (Use GU::LogInterface it other threads).
        
    /**@brief Bundle data that is needed to write down a log.
     * @todo Add a field and a function to save the time when the object is created : not when writed.
     */
    struct logData
    {
        logLevel severity   = logLevel::ERROR;
        const char* name    = "Unkown sender";
        const char* message = "No message attached.";
    };

    /**@brief A thread-safe simple logger.
     * @todo Find a way to isolate the logger from segfault.
     */
    class Logger
    {
    public:
        /**@Brief Create a Logger object.
         * @param "const std::filesystem::path" Path of the dir logs.
         * @param "bool heavyDebug" When set to true, FLUSH EVERY LOG CALL. 
         * Usefull when you seek an segfault, etc ...
         * @warning heavyDebug Make your program run as fast as a Python script.
         */
        Logger(const std::filesystem::path, bool heavyDebug=false); // heavyDebug flush after every log (ULTRA SLOW!).
        ~Logger();

        /**@brief As its name suggests, saves a entry into the current file.
         */
        void log(const logData&&);

        /**@brief Flush, close and reopen the last file to guaranty the entry have been flush.
         * @warning Painfully slow the program, you should use this only if the program is about the crash.
         */
        void flush();
    private:
		bool createLogsDirectories() const noexcept;
		std::string formatDate(const bool filename) const noexcept;
		const char* severityToString(const logLevel) const noexcept;

		std::mutex mutLog;
        const bool flushAfterEveryLogs;
        std::filesystem::path logsDir;
        std::ofstream logsFile;
        std::filesystem::path filePath;
        std::size_t EntryCount;
    };

    // TODO rework the loginterface.
	/**@Brief A simple object that interface with Logger.
     * This interface is made to simplify the call to GU::Logger::log.
	 */
	class LogInterface
	{
	public:
		LogInterface(std::shared_ptr<Logger>, const char* name, bool ignoreOnNullptr=false);
        ~LogInterface();

		void logv(const char*, bool isStringVolatile=false);
		void logi(const char*, bool isStringVolatile=false);
		void logw(const char*, bool isStringVolatile=false);
		void loge(const char*, bool isStringVolatile=false);
		void logf(const char*, bool isStringVolatile=false);

        std::weak_ptr<Logger> getLogger(void) const noexcept;

	private:

        std::weak_ptr<Logger> logger;
		const char* myName;
	};


} //namespace GU
//} //extern C
//#endif//LIBS_GAMEUTILS_LOGGER_HEADER

//#include "logger.hpp"
#include "utilities.hpp"

#include <iomanip>
#include <ctime>
#include <sstream>
#include <stdexcept>

namespace GU
{

    Logger::Logger(const std::filesystem::path dir, const bool heavyDebug)
        : flushAfterEveryLogs(heavyDebug), logsDir(dir), EntryCount(-1) // -1 make the logger open a new file
    {
        if (not createLogsDirectories())
		{
			throw std::runtime_error("GU::Logger failed to create the directories for the logs.");
		}
        log({logLevel::INFO, "Global Logger", "logs start."});
    }

    Logger::~Logger()
    {
        log({logLevel::INFO, "Global Logger", "logs end."});
        //flush();
        logsFile.close();
        //std::cerr<<"DEBUG : Logger ~Logger called"<<std::endl;
    }

	void Logger::log(const logData&& data)
	{
		std::lock_guard<std::mutex> guard(mutLog);

		bool recursiveFailure=false; // goto <3 .
	RETRY:
		if (logsFile)
		{
			if (EntryCount >= LOGGER_MAX_LOGS_ENTRIES_PER_FILE)
			{
				logsFile.flush();
				logsFile.close();
				goto RETRY;
			}
			logsFile<<formatDate(false)<<" : "<<data.name<<" ["<<severityToString(data.severity)<<"] -> "<<data.message<<"\n";
			EntryCount++;
		}
		else if (recursiveFailure)
		{
			std::cerr<<"GU::Logger::log -> Failed to open a log file !!\n";
			throw std::runtime_error("GU::Logger::log -> recursive flag set -> failed to open a file.");
		}
		else
		{
			recursiveFailure=true;
            filePath = logsDir/formatDate(true);
			logsFile.open(filePath);
			EntryCount=0;
			goto RETRY;
		}

        if (flushAfterEveryLogs)
            flush(); // python speed.
	}

    void Logger::flush()
    {
        // close then re-open the file to FORCE the file to be writen.
        if (logsFile.is_open())
        {
            logsFile.flush();
            logsFile.close();
			logsFile.open(filePath, std::ios_base::out | std::ios_base::app);
        }
    }

	const char* Logger::severityToString(const logLevel severity) const noexcept
	{
		switch (severity)
		{
			case (logLevel::VERBOSE):
				return "VERBOSE";
			case (logLevel::INFO):
				return "INFO";
			case (logLevel::WARNING):
				return "WARNING";
			case (logLevel::ERROR):
				return "ERROR";
			case (logLevel::FATAL_ERROR):
				return "FATAL_ERROR";
		}
		return "Unkown severity";
	}

	std::string Logger::formatDate(const bool isFilename) const noexcept
	{
        static size_t fileCount = 0; // Prevent override another file with the
                                     // same name (if two are made in the same
                                     // second).

		std::stringstream sstring;

		std::time_t time;
		std::time(&time);
		std::tm localTime = *std::localtime(&time);

		if (not isFilename)
        {
			sstring<<std::put_time(&localTime, "%T");
            return std::string( sstring.str() );
        }
		else
        {
			sstring<<std::put_time(&localTime, "%c");

            char* filename = GU::duplicateString(sstring.str().c_str());

            size_t lenght = std::strlen(filename);
            for (size_t i(0) ; i<lenght ; i++)
            {
                if (filename[i] == ' ')
                    filename[i] =  '_';
            }

            std::stringstream ss;
            ss<<filename;

            ss<<"("<<fileCount<<")";
            fileCount++;

		    return std::string( ss.str() );
        }
	}

	/**@Brief Create the directories to store the logs.
	 * @Return false if the directories haven't been created.
	 */
	bool Logger::createLogsDirectories() const noexcept
	{
		std::error_code errcode;
		bool recursiveFailure=false;

		RETRY:
		if (not std::filesystem::create_directories(logsDir, errcode))
		{
			if (errcode.value() == 0 && !recursiveFailure) // The directory exist so it's content must by deleted.
			{
				std::filesystem::remove_all(logsDir);
				recursiveFailure = true;
				goto RETRY;
			}
			else if (errcode.value() == 0 && recursiveFailure)
				std::cerr<<"Fatal error for the logger : the log directory is deleted and re-created in loop"<<std::endl;
			else
			{
				std::cerr<<"errcode.value="<<errcode.value()<<std::endl;
				std::cerr<<"errcode.category.name()="<<errcode.category().name()<<std::endl;
				std::cerr<<"errcode.message="<<errcode.message()<<std::endl;
				std::cerr<<"GU::Logger unknown error while creating the logs directories hierarchy.\n";
			}
			return false;
		}
		return true;
	}

	/**@Brief qol interface.
	 * @Except std::invalid_argument if logger is nullptr (unless ignoreOnNullptr is true).
	 */
	LogInterface::LogInterface(std::shared_ptr<Logger> targetLogger, const char* name, bool ignoreOnNullptr)
		: logger(targetLogger), myName(GU::duplicateString(name))
	{
		if ((targetLogger.get() == nullptr) && (ignoreOnNullptr))
			throw std::invalid_argument("GU::LogInterface require a valid Logger* but got nullptr");
	}

    LogInterface::~LogInterface()
    {
        logi("LogInterface deleted.");
        delete myName;
    }

	void LogInterface::logv(const char* msg, bool isStringVolatile)
	{
        const char* savedMsg = nullptr;
        
        if (isStringVolatile) [[unlikely]]
            savedMsg = GU::duplicateString(msg);
        else
            savedMsg = msg;

        if (std::shared_ptr<Logger> spLogger = logger.lock()) // try to access the logger.
		    spLogger->log({GU::logLevel::VERBOSE, myName, savedMsg});
	}
	void LogInterface::logi(const char* msg, bool isStringVolatile)
	{
        const char* savedMsg = nullptr;
        if (isStringVolatile) [[unlikely]]
            savedMsg = GU::duplicateString(msg);
        else
            savedMsg = msg;

        if (std::shared_ptr<Logger> spLogger = logger.lock())
		    spLogger->log({GU::logLevel::INFO, myName, savedMsg});
	}
	void LogInterface::logw(const char* msg, bool isStringVolatile)
	{
        const char* savedMsg = nullptr;
        if (isStringVolatile) [[unlikely]]
            savedMsg = GU::duplicateString(msg);
        else
            savedMsg = msg;

        if (std::shared_ptr<Logger> spLogger = logger.lock()) 
		    spLogger->log({GU::logLevel::WARNING, myName, savedMsg});
	}
	void LogInterface::loge(const char* msg, bool isStringVolatile)
	{
        const char* savedMsg = nullptr;
        if (isStringVolatile) [[unlikely]]
            savedMsg = GU::duplicateString(msg);
        else
            savedMsg = msg;

        if (std::shared_ptr<Logger> spLogger = logger.lock()) 
	        spLogger->log({GU::logLevel::ERROR, myName, savedMsg});
	}
	void LogInterface::logf(const char* msg, bool isStringVolatile)
	{
        const char* savedMsg = nullptr;
        if (isStringVolatile) [[unlikely]]
            savedMsg = GU::duplicateString(msg);
        else
            savedMsg = msg;

        if (std::shared_ptr<Logger> spLogger = logger.lock()) 
        {
		    spLogger->log({GU::logLevel::FATAL_ERROR, myName, savedMsg});
            spLogger->flush();
        }
	}

    std::weak_ptr<Logger> LogInterface::getLogger(void) const noexcept
    {
        return logger;
    }


}//namespace GU

#endif//LIBS_GAMEUTILS_LOGGER_HEADER
