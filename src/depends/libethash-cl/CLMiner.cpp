/// OpenCL miner implementation.
///
/// @file
/// @copyright GNU General Public License

#include "CLMiner.h"
#include "CLMiner_kernel_stable.h"
#include "CLMiner_kernel_experimental.h"

#include <ethash/ethash.hpp>

using namespace dev;
using namespace eth;

namespace dev
{
namespace eth
{

unsigned CLMiner::s_workgroupSize = CLMiner::c_defaultLocalWorkSize;
unsigned CLMiner::s_initialGlobalWorkSize = CLMiner::c_defaultGlobalWorkSizeMultiplier * CLMiner::c_defaultLocalWorkSize;
unsigned CLMiner::s_threadsPerHash = 8;
CLKernelName CLMiner::s_clKernelName = CLMiner::c_defaultKernelName;
bool CLMiner::s_adjustWorkSize = false;

constexpr size_t c_maxSearchResults = 1;

// struct CLChannel: public LogChannel
// {
//     static const char* name() { return EthOrange "cl"; }
//     static const int verbosity = 2;
//     static const bool debug = false;
// };
// #define cllog clog(CLChannel)
// #define ETHCL_LOG(_contents) cllog << _contents
#define ETHCL_LOG(_contents) s_ssLog << _contents
#define cwarn s_ssWarn
#define cnote s_ssNote
#define cllog s_ssLog


/**
 * Returns the name of a numerical cl_int error
 * Takes constants from CL/cl.h and returns them in a readable format
 */
static const char *strClError(cl_int err) {

    switch (err) {
    case CL_SUCCESS:
        return "CL_SUCCESS";
    case CL_DEVICE_NOT_FOUND:
        return "CL_DEVICE_NOT_FOUND";
    case CL_DEVICE_NOT_AVAILABLE:
        return "CL_DEVICE_NOT_AVAILABLE";
    case CL_COMPILER_NOT_AVAILABLE:
        return "CL_COMPILER_NOT_AVAILABLE";
    case CL_MEM_OBJECT_ALLOCATION_FAILURE:
        return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
    case CL_OUT_OF_RESOURCES:
        return "CL_OUT_OF_RESOURCES";
    case CL_OUT_OF_HOST_MEMORY:
        return "CL_OUT_OF_HOST_MEMORY";
    case CL_PROFILING_INFO_NOT_AVAILABLE:
        return "CL_PROFILING_INFO_NOT_AVAILABLE";
    case CL_MEM_COPY_OVERLAP:
        return "CL_MEM_COPY_OVERLAP";
    case CL_IMAGE_FORMAT_MISMATCH:
        return "CL_IMAGE_FORMAT_MISMATCH";
    case CL_IMAGE_FORMAT_NOT_SUPPORTED:
        return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
    case CL_BUILD_PROGRAM_FAILURE:
        return "CL_BUILD_PROGRAM_FAILURE";
    case CL_MAP_FAILURE:
        return "CL_MAP_FAILURE";
    case CL_MISALIGNED_SUB_BUFFER_OFFSET:
        return "CL_MISALIGNED_SUB_BUFFER_OFFSET";
    case CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST:
        return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";

#ifdef CL_VERSION_1_2
    case CL_COMPILE_PROGRAM_FAILURE:
        return "CL_COMPILE_PROGRAM_FAILURE";
    case CL_LINKER_NOT_AVAILABLE:
        return "CL_LINKER_NOT_AVAILABLE";
    case CL_LINK_PROGRAM_FAILURE:
        return "CL_LINK_PROGRAM_FAILURE";
    case CL_DEVICE_PARTITION_FAILED:
        return "CL_DEVICE_PARTITION_FAILED";
    case CL_KERNEL_ARG_INFO_NOT_AVAILABLE:
        return "CL_KERNEL_ARG_INFO_NOT_AVAILABLE";
#endif // CL_VERSION_1_2

    case CL_INVALID_VALUE:
        return "CL_INVALID_VALUE";
    case CL_INVALID_DEVICE_TYPE:
        return "CL_INVALID_DEVICE_TYPE";
    case CL_INVALID_PLATFORM:
        return "CL_INVALID_PLATFORM";
    case CL_INVALID_DEVICE:
        return "CL_INVALID_DEVICE";
    case CL_INVALID_CONTEXT:
        return "CL_INVALID_CONTEXT";
    case CL_INVALID_QUEUE_PROPERTIES:
        return "CL_INVALID_QUEUE_PROPERTIES";
    case CL_INVALID_COMMAND_QUEUE:
        return "CL_INVALID_COMMAND_QUEUE";
    case CL_INVALID_HOST_PTR:
        return "CL_INVALID_HOST_PTR";
    case CL_INVALID_MEM_OBJECT:
        return "CL_INVALID_MEM_OBJECT";
    case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR:
        return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
    case CL_INVALID_IMAGE_SIZE:
        return "CL_INVALID_IMAGE_SIZE";
    case CL_INVALID_SAMPLER:
        return "CL_INVALID_SAMPLER";
    case CL_INVALID_BINARY:
        return "CL_INVALID_BINARY";
    case CL_INVALID_BUILD_OPTIONS:
        return "CL_INVALID_BUILD_OPTIONS";
    case CL_INVALID_PROGRAM:
        return "CL_INVALID_PROGRAM";
    case CL_INVALID_PROGRAM_EXECUTABLE:
        return "CL_INVALID_PROGRAM_EXECUTABLE";
    case CL_INVALID_KERNEL_NAME:
        return "CL_INVALID_KERNEL_NAME";
    case CL_INVALID_KERNEL_DEFINITION:
        return "CL_INVALID_KERNEL_DEFINITION";
    case CL_INVALID_KERNEL:
        return "CL_INVALID_KERNEL";
    case CL_INVALID_ARG_INDEX:
        return "CL_INVALID_ARG_INDEX";
    case CL_INVALID_ARG_VALUE:
        return "CL_INVALID_ARG_VALUE";
    case CL_INVALID_ARG_SIZE:
        return "CL_INVALID_ARG_SIZE";
    case CL_INVALID_KERNEL_ARGS:
        return "CL_INVALID_KERNEL_ARGS";
    case CL_INVALID_WORK_DIMENSION:
        return "CL_INVALID_WORK_DIMENSION";
    case CL_INVALID_WORK_GROUP_SIZE:
        return "CL_INVALID_WORK_GROUP_SIZE";
    case CL_INVALID_WORK_ITEM_SIZE:
        return "CL_INVALID_WORK_ITEM_SIZE";
    case CL_INVALID_GLOBAL_OFFSET:
        return "CL_INVALID_GLOBAL_OFFSET";
    case CL_INVALID_EVENT_WAIT_LIST:
        return "CL_INVALID_EVENT_WAIT_LIST";
    case CL_INVALID_EVENT:
        return "CL_INVALID_EVENT";
    case CL_INVALID_OPERATION:
        return "CL_INVALID_OPERATION";
    case CL_INVALID_GL_OBJECT:
        return "CL_INVALID_GL_OBJECT";
    case CL_INVALID_BUFFER_SIZE:
        return "CL_INVALID_BUFFER_SIZE";
    case CL_INVALID_MIP_LEVEL:
        return "CL_INVALID_MIP_LEVEL";
    case CL_INVALID_GLOBAL_WORK_SIZE:
        return "CL_INVALID_GLOBAL_WORK_SIZE";
    case CL_INVALID_PROPERTY:
        return "CL_INVALID_PROPERTY";

#ifdef CL_VERSION_1_2
    case CL_INVALID_IMAGE_DESCRIPTOR:
        return "CL_INVALID_IMAGE_DESCRIPTOR";
    case CL_INVALID_COMPILER_OPTIONS:
        return "CL_INVALID_COMPILER_OPTIONS";
    case CL_INVALID_LINKER_OPTIONS:
        return "CL_INVALID_LINKER_OPTIONS";
    case CL_INVALID_DEVICE_PARTITION_COUNT:
        return "CL_INVALID_DEVICE_PARTITION_COUNT";
#endif // CL_VERSION_1_2

#ifdef CL_VERSION_2_0
    case CL_INVALID_PIPE_SIZE:
        return "CL_INVALID_PIPE_SIZE";
    case CL_INVALID_DEVICE_QUEUE:
        return "CL_INVALID_DEVICE_QUEUE";
#endif // CL_VERSION_2_0

#ifdef CL_VERSION_2_2
    case CL_INVALID_SPEC_ID:
        return "CL_INVALID_SPEC_ID";
    case CL_MAX_SIZE_RESTRICTION_EXCEEDED:
        return "CL_MAX_SIZE_RESTRICTION_EXCEEDED";
#endif // CL_VERSION_2_2
    }

    return "Unknown CL error encountered";
}

/**
 * Prints cl::Errors in a uniform way
 * @param msg text prepending the error message
 * @param clerr cl:Error object
 *
 * Prints errors in the format:
 *      msg: what(), string err() (numeric err())
 */
static std::string ethCLErrorHelper(const char *msg, cl::Error const &clerr) {
    std::ostringstream osstream;
    osstream << msg << ": " << clerr.what() << ": " << strClError(clerr.err())
             << " (" << clerr.err() << ")";
    return osstream.str();
}

namespace
{

void addDefinition(std::string& _source, char const* _id, unsigned _value)
{
    char buf[256];
    sprintf(buf, "#define %s %uu\n", _id, _value);
    _source.insert(_source.begin(), buf, buf + strlen(buf));
}

std::vector<cl::Platform> getPlatforms()
{
    std::vector<cl::Platform> platforms;
    try
    {
        cl::Platform::get(&platforms);
    }
    catch(cl::Error const& err)
    {
#if defined(CL_PLATFORM_NOT_FOUND_KHR)
        if (err.err() == CL_PLATFORM_NOT_FOUND_KHR)
            std::cout << "No OpenCL platforms found";
        else
#endif
            throw err;
    }
    return platforms;
}

std::vector<cl::Device> getDevices(std::vector<cl::Platform> const& _platforms, unsigned _platformId)
{
    std::vector<cl::Device> devices;
    size_t platform_num = std::min<size_t>(_platformId, _platforms.size() - 1);
    try
    {
        _platforms[platform_num].getDevices(
            CL_DEVICE_TYPE_GPU | CL_DEVICE_TYPE_ACCELERATOR,
            &devices
        );
    }
    catch (cl::Error const& err)
    {
        // if simply no devices found return empty vector
        if (err.err() != CL_DEVICE_NOT_FOUND)
            throw err;
    }
    return devices;
}

}

}
}

unsigned CLMiner::s_platformId = 0;
unsigned CLMiner::s_numInstances = 0;
std::vector<int> CLMiner::s_devices(MAX_MINERS, -1);

CLMiner::~CLMiner()
{
}

typedef struct {
    unsigned count;
    unsigned gid;
    unsigned mix[8];
} search_results;

bool CLMiner::mine(const WorkPackage &w, Solution &solution)
{
    // Memory for zero-ing buffers. Cannot be static because crashes on macOS.
    uint32_t const c_zero = 0;

    // The work package currently processed by GPU.
    bool result = true;
    try {
        if (m_currentWP.header != w.header)
        {
            if (m_currentWP.blockNumber / ETHASH_EPOCH_LENGTH != w.blockNumber / ETHASH_EPOCH_LENGTH)
            {
                if (s_dagLoadMode == DAG_LOAD_MODE_SEQUENTIAL)
                {
                    while (s_dagLoadIndex < m_index)
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    ++s_dagLoadIndex;
                }

                if(!init(w.blockNumber))
                {
                    return false;
                }
            }

            // Upper 64 bits of the boundary.
            const uint64_t target = (uint64_t)(u64)((u256)w.boundary >> 192);
            assert(target > 0);

            // Update header constant buffer.
            m_queue.enqueueWriteBuffer(m_header, CL_FALSE, 0, w.header.size, w.header.data());
            m_queue.enqueueWriteBuffer(m_searchBuffer, CL_FALSE, 0, sizeof(c_zero), &c_zero);

            m_searchKernel.setArg(0, m_searchBuffer);  // Supply output buffer to kernel.
            m_searchKernel.setArg(4, target);
        }

        // Read results.
        search_results results;

        m_queue.enqueueReadBuffer(m_searchBuffer, CL_TRUE, 0, sizeof(results), &results);

        if (results.count)
        {
            // Reset search buffer if any solution found.
            m_queue.enqueueWriteBuffer(m_searchBuffer, CL_FALSE, 0, sizeof(c_zero), &c_zero);
        }

        // Run the kernel.
        m_searchKernel.setArg(3, w.startNonce);
        m_queue.enqueueNDRangeKernel(
            m_searchKernel, cl::NullRange, m_globalWorkSize, m_workgroupSize, nullptr, &m_event);
        m_event.wait();
        
        m_queue.enqueueReadBuffer(m_searchBuffer, CL_TRUE, 0, sizeof(results), &results);

        // Report results while the kernel is running.
        if (results.count)
        {
            uint64_t nonce = w.startNonce + results.gid;

            h256 mix;
            memcpy(mix.data(), results.mix, sizeof(results.mix));
            solution
                = Solution{nonce, mix, m_currentWP.header != w.header};
        }else 
            solution.nonce = w.startNonce + m_globalWorkSize;

        m_currentWP = w;  // kernel now processing newest work

        // Report hash count
        addHashCount(m_globalWorkSize);

        // Make sure the last buffer write has finished --
        // it reads local variable.
        m_queue.finish();
    }
    catch (cl::Error const& _e)
    {
        cwarn << ethCLErrorHelper("OpenCL Error", _e);
        if(s_exit)
            exit(1);
    }
    return result;
}

unsigned CLMiner::getNumDevices()
{
    std::vector<cl::Platform> platforms = getPlatforms();
    if (platforms.empty())
        return 0;

    std::vector<cl::Device> devices = getDevices(platforms, s_platformId);
    if (devices.empty())
    {
        cwarn << "No OpenCL devices found.";
        return 0;
    }
    return devices.size();
}

void CLMiner::listDevices()
{
    std::string outString ="\nListing OpenCL devices.\nFORMAT: [platformID] [deviceID] deviceName\n";
    unsigned int i = 0;

    std::vector<cl::Platform> platforms = getPlatforms();
    if (platforms.empty())
        return;
    for (unsigned j = 0; j < platforms.size(); ++j)
    {
        i = 0;
        std::vector<cl::Device> devices = getDevices(platforms, j);
        for (auto const& device: devices)
        {
            outString += "[" + std::to_string(j) + "] [" + std::to_string(i) + "] " + device.getInfo<CL_DEVICE_NAME>() + "\n";
            outString += "\tCL_DEVICE_TYPE: ";
            switch (device.getInfo<CL_DEVICE_TYPE>())
            {
            case CL_DEVICE_TYPE_CPU:
                outString += "CPU\n";
                break;
            case CL_DEVICE_TYPE_GPU:
                outString += "GPU\n";
                break;
            case CL_DEVICE_TYPE_ACCELERATOR:
                outString += "ACCELERATOR\n";
                break;
            default:
                outString += "DEFAULT\n";
                break;
            }
            outString += "\tCL_DEVICE_GLOBAL_MEM_SIZE: " + std::to_string(device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>()) + "\n";
            outString += "\tCL_DEVICE_MAX_MEM_ALLOC_SIZE: " + std::to_string(device.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>()) + "\n";
            outString += "\tCL_DEVICE_MAX_WORK_GROUP_SIZE: " + std::to_string(device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>()) + "\n";
            ++i;
        }
    }
    std::cout << outString;
}

bool CLMiner::configureGPU(
    unsigned _localWorkSize,
    int _globalWorkSizeMultiplier,
    unsigned _platformId,
    int epoch,
    unsigned _dagLoadMode,
    unsigned _dagCreateDevice,
	bool _noeval,
    bool _exit
    )
{
	s_noeval = _noeval;
    s_dagLoadMode = _dagLoadMode;
    s_dagCreateDevice = _dagCreateDevice;
    s_exit = _exit;

    s_platformId = _platformId;

    _localWorkSize = ((_localWorkSize + 7) / 8) * 8;
    s_workgroupSize = _localWorkSize;
	if (_globalWorkSizeMultiplier < 0) {
		s_adjustWorkSize = true;
		_globalWorkSizeMultiplier = -_globalWorkSizeMultiplier;
	}
    s_initialGlobalWorkSize = _globalWorkSizeMultiplier * _localWorkSize;

    auto dagSize = ethash::get_full_dataset_size(ethash::calculate_full_dataset_num_items(epoch));

    std::vector<cl::Platform> platforms = getPlatforms();
    if (platforms.empty())
        return false;
    if (_platformId >= platforms.size())
        return false;

    std::vector<cl::Device> devices = getDevices(platforms, _platformId);
    bool foundSuitableDevice = false;
    for (auto const& device: devices)
    {
        cl_ulong result = 0;
        device.getInfo(CL_DEVICE_GLOBAL_MEM_SIZE, &result);
        if (result >= dagSize)
        {
            cnote <<
                "Found suitable OpenCL device [" << device.getInfo<CL_DEVICE_NAME>()
                                                 << "] with " << result << " bytes of GPU memory";
            foundSuitableDevice = true;
        } 
        else 
        {
            cnote <<
                "OpenCL device " << device.getInfo<CL_DEVICE_NAME>()
                             << " has insufficient GPU memory." << result <<
                             " bytes of memory found < " << dagSize << " bytes of memory required";
        }
    }
    if (foundSuitableDevice)
    {
        return true;
    }
    std::cout << "No GPU device with sufficient memory was found" << std::endl;
    return false;
}

bool CLMiner::init(uint64_t blockNumber)
{
    // get all platforms
    try
    {
        std::vector<cl::Platform> platforms = getPlatforms();
        if (platforms.empty())
            return false;

        // use selected platform
        unsigned platformIdx = std::min<unsigned>(s_platformId, platforms.size() - 1);

        std::string platformName = platforms[platformIdx].getInfo<CL_PLATFORM_NAME>();
        ETHCL_LOG("Platform: " << platformName);

        int platformId = OPENCL_PLATFORM_UNKNOWN;
        {
            // this mutex prevents race conditions when calling the adl wrapper since it is apparently not thread safe
            static std::mutex mtx;
            std::lock_guard<std::mutex> lock(mtx);

            if (platformName == "NVIDIA CUDA")
            {
                platformId = OPENCL_PLATFORM_NVIDIA;
                m_hwmoninfo.deviceType = HwMonitorInfoType::NVIDIA;
                m_hwmoninfo.indexSource = HwMonitorIndexSource::OPENCL;
            }
            else if (platformName == "AMD Accelerated Parallel Processing")
            {
                platformId = OPENCL_PLATFORM_AMD;
                m_hwmoninfo.deviceType = HwMonitorInfoType::AMD;
                m_hwmoninfo.indexSource = HwMonitorIndexSource::OPENCL;
            }
            else if (platformName == "Clover")
            {
                platformId = OPENCL_PLATFORM_CLOVER;
            }
        }

        // get GPU device of the default platform
        std::vector<cl::Device> devices = getDevices(platforms, platformIdx);
        if (devices.empty())
        {
            ETHCL_LOG("No OpenCL devices found.");
            return false;
        }

        // use selected device
        int idx = m_index % devices.size();
        unsigned deviceId = s_devices[idx] > -1 ? s_devices[idx] : m_index;
        m_hwmoninfo.deviceIndex = deviceId % devices.size();
        cl::Device& device = devices[deviceId % devices.size()];
        std::string device_version = device.getInfo<CL_DEVICE_VERSION>();
        ETHCL_LOG("Device:   " << device.getInfo<CL_DEVICE_NAME>() << " / " << device_version);

        std::string clVer = device_version.substr(7, 3);
        if (clVer == "1.0" || clVer == "1.1")
        {
            if (platformId == OPENCL_PLATFORM_CLOVER)
            {
                ETHCL_LOG("OpenCL " << clVer << " not supported, but platform Clover might work nevertheless. USE AT OWN RISK!");
            }
            else
            {
                ETHCL_LOG("OpenCL " << clVer << " not supported - minimum required version is 1.2");
                return false;
            }
        }

        char options[256];
        int computeCapability = 0;
        if (platformId == OPENCL_PLATFORM_NVIDIA) {
            cl_uint computeCapabilityMajor;
            cl_uint computeCapabilityMinor;
            clGetDeviceInfo(device(), CL_DEVICE_COMPUTE_CAPABILITY_MAJOR_NV, sizeof(cl_uint), &computeCapabilityMajor, nullptr);
            clGetDeviceInfo(device(), CL_DEVICE_COMPUTE_CAPABILITY_MINOR_NV, sizeof(cl_uint), &computeCapabilityMinor, nullptr);

            computeCapability = computeCapabilityMajor * 10 + computeCapabilityMinor;
            int maxregs = computeCapability >= 35 ? 72 : 63;
            sprintf(options, "-cl-nv-maxrregcount=%d", maxregs);
        }
        else {
            sprintf(options, "%s", "");
        }
        // create context
        m_context = cl::Context(std::vector<cl::Device>(&device, &device + 1));
        m_queue = cl::CommandQueue(m_context, device);

        m_workgroupSize = s_workgroupSize;
        m_globalWorkSize = s_initialGlobalWorkSize;

        if (s_adjustWorkSize) {
            unsigned int computeUnits;
	    	clGetDeviceInfo(device(), CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(computeUnits), &computeUnits, nullptr);
            // Apparently some 36 CU devices return a bogus 14!!!
            computeUnits = computeUnits == 14 ? 36 : computeUnits;
		    if ((platformId == OPENCL_PLATFORM_AMD) && (computeUnits != 36)) {
		    	m_globalWorkSize = (m_globalWorkSize * computeUnits) / 36;
		    	// make sure that global work size is evenly divisible by the local workgroup size
		    	if (m_globalWorkSize % m_workgroupSize != 0)
		    		m_globalWorkSize = ((m_globalWorkSize / m_workgroupSize) + 1) * m_workgroupSize;
		    	  cnote << "Adjusting CL work multiplier for " << computeUnits << " CUs."
		    		<< "Adjusted work multiplier: " << m_globalWorkSize / m_workgroupSize;
		      }
        }

        const auto epoch = ethash::get_epoch_number(blockNumber);
        const auto& context = ethash::get_global_epoch_context(epoch);
        const auto lightNumItems = context.light_cache_num_items;
        const auto lightSize = ethash::get_light_cache_size(lightNumItems);
        auto dagNumItems = context.full_dataset_num_items;
        const auto dagSize = ethash::get_full_dataset_size(dagNumItems);

        //const auto lightNumItems = lightSize / ETHASH_HASH_BYTES;
        //const auto dagSize = ethash_get_datasize(blockNumber);
        //const auto dagNumItems = dagSize / ETHASH_MIX_BYTES;
        // patch source code
        // note: The kernels here are simply compiled version of the respective .cl kernels
        // into a byte array by bin2h.cmake. There is no need to load the file by hand in runtime
        // See libethash-cl/CMakeLists.txt: add_custom_command()
        // TODO: Just use C++ raw string literal.
        std::string code;

        if ( s_clKernelName == CLKernelName::Experimental ) {
            cllog << "OpenCL kernel: Experimental kernel";
            code = std::string(CLMiner_kernel_experimental, CLMiner_kernel_experimental + sizeof(CLMiner_kernel_experimental));
        }
        else { //if(s_clKernelName == CLKernelName::Stable)
            cllog << "OpenCL kernel: Stable kernel";

            //CLMiner_kernel_stable.cl will do a #undef THREADS_PER_HASH
            if(s_threadsPerHash != 8) {
                cwarn << "The current stable OpenCL kernel only supports exactly 8 threads. Thread parameter will be ignored.";
            }

            code = std::string(CLMiner_kernel_stable, CLMiner_kernel_stable + sizeof(CLMiner_kernel_stable));
        }
        addDefinition(code, "GROUP_SIZE", m_workgroupSize);
        addDefinition(code, "DAG_SIZE", dagNumItems);
        addDefinition(code, "LIGHT_SIZE", lightNumItems);
        addDefinition(code, "ACCESSES", 64);
        addDefinition(code, "MAX_OUTPUTS", c_maxSearchResults);
        addDefinition(code, "PLATFORM", platformId);
        addDefinition(code, "COMPUTE", computeCapability);
        addDefinition(code, "THREADS_PER_HASH", s_threadsPerHash);

        // create miner OpenCL program
        cl::Program::Sources sources{{code.data(), code.size()}};
        cl::Program program(m_context, sources);
        try
        {
            program.build({device}, options);
        }
        catch (cl::BuildError const& buildErr)
        {
            cwarn << "OpenCL kernel build log:\n"
                  << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
            cwarn << "OpenCL kernel build error (" << buildErr.err() << "):\n" << buildErr.what();
            return false;
        }

        m_event = cl::UserEvent(m_context);

        //check whether the current dag fits in memory everytime we recreate the DAG
        cl_ulong result = 0;
        device.getInfo(CL_DEVICE_GLOBAL_MEM_SIZE, &result);
        if (result < dagSize)
        {
            cnote <<
            "OpenCL device " << device.getInfo<CL_DEVICE_NAME>()
                             << " has insufficient GPU memory." << result <<
                             " bytes of memory found < " << dagSize << " bytes of memory required";    
            return false;
        }

        // create buffer for dag
        try
        {
            cllog << "Creating light cache buffer, size: " << lightSize;
            m_light = cl::Buffer(m_context, CL_MEM_READ_ONLY, lightSize);
            cllog << "Creating DAG buffer, size: " << dagSize;
            m_dag = cl::Buffer(m_context, CL_MEM_READ_ONLY, dagSize);
            cllog << "Loading kernels";
            m_searchKernel = cl::Kernel(program, "ethash_search");
            m_dagKernel = cl::Kernel(program, "ethash_calculate_dag_item");
            cllog << "Writing light cache buffer";

            m_queue.enqueueWriteBuffer(m_light, CL_TRUE, 0, lightSize, context.light_cache);
        }
        catch (cl::Error const& err)
        {
            cwarn << ethCLErrorHelper("Creating DAG buffer failed", err);
            return false;
        }
        // create buffer for header
        ETHCL_LOG("Creating buffer for header.");
        m_header = cl::Buffer(m_context, CL_MEM_READ_ONLY, 32);

        m_searchKernel.setArg(1, m_header);
        m_searchKernel.setArg(2, m_dag);
        m_searchKernel.setArg(5, ~0u);  // Pass this to stop the compiler unrolling the loops.

        // create mining buffers
        ETHCL_LOG("Creating mining buffer");
        m_searchBuffer = cl::Buffer(m_context, CL_MEM_WRITE_ONLY, sizeof(search_results));

        const auto workItems = dagNumItems * 2;  // GPU computes partial 512-bit DAG items.
        uint32_t fullRuns = workItems / m_globalWorkSize;
        uint32_t const restWork = workItems % m_globalWorkSize;
        if (restWork > 0) fullRuns++;

        m_dagKernel.setArg(1, m_light);
        m_dagKernel.setArg(2, m_dag);
        m_dagKernel.setArg(3, ~0u);

        auto startDAG = std::chrono::steady_clock::now();
        for (uint32_t i = 0; i < fullRuns; i++)
        {
            m_dagKernel.setArg(0, i * m_globalWorkSize);
            m_queue.enqueueNDRangeKernel(m_dagKernel, cl::NullRange, m_globalWorkSize, m_workgroupSize);
            m_queue.finish();
        }
        auto endDAG = std::chrono::steady_clock::now();

        auto dagTime = std::chrono::duration_cast<std::chrono::milliseconds>(endDAG-startDAG);
        float gb = (float)dagSize / (1024 * 1024 * 1024);
        cnote << gb << " GB of DAG data generated in " << dagTime.count() << " ms.";
    }
    catch (cl::Error const& err)
    {
        cwarn << ethCLErrorHelper("OpenCL init failed", err);
        if(s_exit)
            exit(1);
        return false;
    }
    return true;
}
