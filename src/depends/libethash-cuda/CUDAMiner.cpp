/*
This file is part of cpp-ethereum.

cpp-ethereum is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

cpp-ethereum is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "CUDAMiner.h"

#include "libethash/internal.h"
#include "libethash/ethash.h"

using namespace std;
using namespace dev;
using namespace eth;

unsigned CUDAMiner::s_numInstances = 0;

vector<int> CUDAMiner::s_devices(MAX_MINERS, -1);

#define cudalog s_ssLog
#define cwarn s_ssWarn
#define cnote s_ssNote
#define cllog s_ssLog

CUDAMiner::CUDAMiner() : m_light(getNumDevices()) {}

CUDAMiner::~CUDAMiner()
{
}

bool CUDAMiner::init(uint64_t blockNumber)
{
    try {
        if (s_dagLoadMode == DAG_LOAD_MODE_SEQUENTIAL)
            while (s_dagLoadIndex < index)
                this_thread::sleep_for(chrono::milliseconds(100));
        unsigned device = s_devices[index] > -1 ? s_devices[index] : index;

        cnote << "Initialising miner " << index;

        cuda_init(getNumDevices(), blockNumber, device, (s_dagLoadMode == DAG_LOAD_MODE_SINGLE),
            s_dagInHostMemory, s_dagCreateDevice);
        s_dagLoadIndex++;
    
        if (s_dagLoadMode == DAG_LOAD_MODE_SINGLE)
        {
            if (s_dagLoadIndex >= s_numInstances && s_dagInHostMemory)
            {
                // all devices have loaded DAG, we can free now
                delete[] s_dagInHostMemory;
                s_dagInHostMemory = nullptr;
                cnote << "Freeing DAG from host";
            }
        }
        return true;
    }
    catch (std::runtime_error const& _e)
    {
        cwarn << "Error CUDA mining: " << _e.what();
        if(s_exit)
            exit(1);
        return false;
    }
}

void CUDAMiner::setNumInstances(unsigned _instances)
{
    s_numInstances = std::min<unsigned>(_instances, getNumDevices());
}

void CUDAMiner::setDevices(const vector<unsigned>& _devices, unsigned _selectedDeviceCount)
{
    for (unsigned i = 0; i < _selectedDeviceCount; i++)
        s_devices[i] = _devices[i];
}

unsigned CUDAMiner::getNumDevices()
{
    int deviceCount;
    cudaError_t err = cudaGetDeviceCount(&deviceCount);
    if (err == cudaSuccess)
        return deviceCount;

    if (err == cudaErrorInsufficientDriver)
    {
        int driverVersion;
        cudaDriverGetVersion(&driverVersion);
        if (driverVersion == 0)
            throw std::runtime_error{"No CUDA driver found"};
        throw std::runtime_error{"Insufficient CUDA driver: " + std::to_string(driverVersion)};
    }

    throw std::runtime_error{cudaGetErrorString(err)};
}

void CUDAMiner::listDevices()
{
    try
    {
        cout << "\nListing CUDA devices.\nFORMAT: [deviceID] deviceName\n";
        int numDevices = getNumDevices();
        for (int i = 0; i < numDevices; ++i)
        {
            cudaDeviceProp props;
            CUDA_SAFE_CALL(cudaGetDeviceProperties(&props, i));

            cout << "[" + to_string(i) + "] " + string(props.name) + "\n";
            cout << "\tCompute version: " + to_string(props.major) + "." + to_string(props.minor) + "\n";
            cout << "\tcudaDeviceProp::totalGlobalMem: " + to_string(props.totalGlobalMem) + "\n";
            cout << "\tPci: " << setw(4) << setfill('0') << hex << props.pciDomainID << ':' << setw(2)
                << props.pciBusID << ':' << setw(2) << props.pciDeviceID << '\n';
        }
    }
    catch(std::runtime_error const& err)
    {
        cwarn << "CUDA error: " << err.what();
        if(s_exit)
            exit(1);
    }
}

bool CUDAMiner::configureGPU(
    unsigned _blockSize,
    unsigned _gridSize,
    unsigned _numStreams,
    unsigned _scheduleFlag,
    unsigned _dagLoadMode,
    unsigned _dagCreateDevice,
    bool _noeval,
    bool _exit
    )
{
    s_dagLoadMode = _dagLoadMode;
    s_dagCreateDevice = _dagCreateDevice;
    s_exit  = _exit;

    if (!cuda_configureGPU(
        getNumDevices(),
        s_devices,
        ((_blockSize + 7) / 8) * 8,
        _gridSize,
        _numStreams,
        _scheduleFlag,
        _noeval)
        )
    {
        cout << "No CUDA device with sufficient memory was found. Can't CUDA mine. Remove the -U argument" << endl;
        return false;
    }
    return true;
}

void CUDAMiner::setParallelHash(unsigned _parallelHash)
{
      m_parallelHash = _parallelHash;
}

unsigned const CUDAMiner::c_defaultBlockSize = 128;
unsigned const CUDAMiner::c_defaultGridSize = 8192; // * CL_DEFAULT_LOCAL_WORK_SIZE
unsigned const CUDAMiner::c_defaultNumStreams = 2;

bool CUDAMiner::cuda_configureGPU(
    size_t numDevices,
    const vector<int>& _devices,
    unsigned _blockSize,
    unsigned _gridSize,
    unsigned _numStreams,
    unsigned _scheduleFlag,
    bool _noeval
    )
{
    try
    {
        s_blockSize = _blockSize;
        s_gridSize = _gridSize;
        s_numStreams = _numStreams;
        s_scheduleFlag = _scheduleFlag;
        s_noeval = _noeval;

        cudalog << "Using grid size: " << s_gridSize << ", block size: " << s_blockSize;

        // by default let's only consider the DAG of the first epoch
        const auto dagSize = ethash_get_datasize(0); // ethash::get_full_dataset_size(ethash::calculate_full_dataset_num_items(0));
        int devicesCount = static_cast<int>(numDevices);
        for (int i = 0; i < devicesCount; i++)
        {
            if (_devices[i] != -1)
            {
                int deviceId = min(devicesCount - 1, _devices[i]);
                cudaDeviceProp props;
                CUDA_SAFE_CALL(cudaGetDeviceProperties(&props, deviceId));
                if (props.totalGlobalMem >= dagSize)
                {
                    cudalog <<  "Found suitable CUDA device [" << string(props.name) << "] with " << props.totalGlobalMem << " bytes of GPU memory";
                }
                else
                {
                    cudalog <<  "CUDA device " << string(props.name) << " has insufficient GPU memory. " << props.totalGlobalMem << " bytes of memory found < " << dagSize << " bytes of memory required";
                    return false;
                }
            }
        }
        return true;
    }
    catch (runtime_error)
    {
        if(s_exit)
            exit(1);
        return false;
    }
}

unsigned CUDAMiner::m_parallelHash = 4;
unsigned CUDAMiner::s_blockSize = CUDAMiner::c_defaultBlockSize;
unsigned CUDAMiner::s_gridSize = CUDAMiner::c_defaultGridSize;
unsigned CUDAMiner::s_numStreams = CUDAMiner::c_defaultNumStreams;
unsigned CUDAMiner::s_scheduleFlag = 0;

bool CUDAMiner::cuda_init(
    size_t numDevices,
    uint64_t blockNumber,
    unsigned _deviceId,
    bool _cpyToHost,
    uint8_t* &hostDAG,
    unsigned dagCreateDevice)
{
    try
    {
        if (numDevices == 0)
            return false;

        // use selected device
        m_device_num = _deviceId < numDevices -1 ? _deviceId : numDevices - 1;
        m_hwmoninfo.deviceType = HwMonitorInfoType::NVIDIA;
        m_hwmoninfo.indexSource = HwMonitorIndexSource::CUDA;
        m_hwmoninfo.deviceIndex = m_device_num;

        cudaDeviceProp device_props;
        CUDA_SAFE_CALL(cudaGetDeviceProperties(&device_props, m_device_num));

        cudalog << "Using device: " << device_props.name << " (Compute " + to_string(device_props.major) + "." + to_string(device_props.minor) + ")";

        m_search_buf = new volatile search_results *[s_numStreams];
        m_streams = new cudaStream_t[s_numStreams];

        const auto lightSize = ethash_get_cachesize(blockNumber);
        const auto lightNumItems = lightSize / ETHASH_HASH_BYTES;
        const auto dagSize = ethash_get_datasize(blockNumber);
        const int dagNumItems = dagSize / ETHASH_MIX_BYTES;

        CUDA_SAFE_CALL(cudaSetDevice(m_device_num));
        cudalog << "Set Device to current";
        if ((int)dagNumItems != m_dag_size || !m_dag)
        {
            //Check whether the current device has sufficient memory every time we recreate the dag
            if (device_props.totalGlobalMem < dagSize)
            {
                cudalog <<  "CUDA device " << string(device_props.name) << " has insufficient GPU memory. " << device_props.totalGlobalMem << " bytes of memory found < " << dagSize << " bytes of memory required";
                return false;
            }
            //We need to reset the device and recreate the dag  
            cudalog << "Resetting device";
            CUDA_SAFE_CALL(cudaDeviceReset());
            CUDA_SAFE_CALL(cudaSetDeviceFlags(s_scheduleFlag));
            CUDA_SAFE_CALL(cudaDeviceSetCacheConfig(cudaFuncCachePreferL1));
            //We need to reset the light and the Dag for the following code to reallocate
            //since cudaDeviceReset() frees all previous allocated memory
            m_light[m_device_num] = nullptr;
            m_dag = nullptr; 
        }
        // create buffer for cache
        hash128_t * dag = m_dag;
        hash64_t * light = m_light[m_device_num];

        if(!light){ 
            cudalog << "Allocating light with size: " << lightSize;
            CUDA_SAFE_CALL(cudaMalloc(reinterpret_cast<void**>(&light), lightSize));
        }
        // copy lightData to device
        auto ethash_light_client = ethash_light_new(blockNumber);
        CUDA_SAFE_CALL(cudaMemcpy(reinterpret_cast<void*>(light), ethash_light_client->cache, lightSize, cudaMemcpyHostToDevice));
        ethash_light_delete(ethash_light_client);
        m_light[m_device_num] = light;

        if (dagNumItems != m_dag_size || !dag)  // create buffer for dag
            CUDA_SAFE_CALL(cudaMalloc(reinterpret_cast<void**>(&dag), dagSize));
            
        set_constants(dag, dagNumItems, light, lightNumItems); //in ethash_cuda_miner_kernel.cu

        if (dagNumItems != m_dag_size || !dag)
        {
            // create mining buffers
            cudalog << "Generating mining buffers";
            for (unsigned i = 0; i != s_numStreams; ++i)
            {
                CUDA_SAFE_CALL(cudaMallocHost(&m_search_buf[i], sizeof(search_results)));
                CUDA_SAFE_CALL(cudaStreamCreateWithFlags(&m_streams[i], cudaStreamNonBlocking));
            }

            m_current_target = 0;

            if (!hostDAG)
            {
                if((m_device_num == dagCreateDevice) || !_cpyToHost){ //if !cpyToHost -> All devices shall generate their DAG
                    cudalog << "Generating DAG for GPU #" << m_device_num << " with dagSize: " 
                            << dagSize <<" gridSize: " << s_gridSize;
			        auto startDAG = std::chrono::steady_clock::now();

                    ethash_generate_dag(dagSize, s_gridSize, s_blockSize, m_streams[0]);

                    cudalog << "Generated DAG for GPU" << m_device_num << " in: "
						<< std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startDAG).count()
						<< " ms.";

                    if (_cpyToHost)
                    {
                        uint8_t* memoryDAG = new uint8_t[dagSize];
                        cudalog << "Copying DAG from GPU #" << m_device_num << " to host";
                        CUDA_SAFE_CALL(cudaMemcpy(reinterpret_cast<void*>(memoryDAG), dag, dagSize, cudaMemcpyDeviceToHost));

                        hostDAG = memoryDAG;
                    }
                }else{
                    while(!hostDAG)
                        this_thread::sleep_for(chrono::milliseconds(100)); 
                    goto cpyDag;
                }
            }
            else
            {
cpyDag:
                cudalog << "Copying DAG from host to GPU #" << m_device_num;
                const void* hdag = (const void*)hostDAG;
                CUDA_SAFE_CALL(cudaMemcpy(reinterpret_cast<void*>(dag), hdag, dagSize, cudaMemcpyHostToDevice));
            }
        }
    
        m_dag = dag;
        m_dag_size = dagNumItems;
        return true;
    }
    catch (runtime_error const&)
    {
        if(s_exit)
            exit(1);
        return false;
    }
}

void CUDAMiner::search(
    uint8_t const* header,
    uint64_t target,
    const WorkPackage& w,
    Solution &solution)
{
    set_header(*reinterpret_cast<hash32_t const *>(header));
    if (m_current_target != target) {
        set_target(target);
        m_current_target = target;
    }

    // choose the starting nonce
    uint64_t current_nonce = w.startNonce;

    // Nonces processed in one pass by a single stream
    const uint32_t batch_size = s_gridSize * s_blockSize;
    // Nonces processed in one pass by all streams
    const uint32_t streams_batch_size = batch_size * s_numStreams;
    volatile search_results* buffer;

    // prime each stream and clear search result buffers
    uint32_t current_index;
    for (current_index = 0; current_index < s_numStreams;
         current_index++, current_nonce += batch_size)
    {
        cudaStream_t stream = m_streams[current_index];
        buffer = m_search_buf[current_index];
        buffer->count = 0;

        // Run the batch for this stream
        run_ethash_search(s_gridSize, s_blockSize, stream, buffer, current_nonce, m_parallelHash);
    }

    uint32_t found_count = 0;

    for (current_index = 0; current_index < s_numStreams;
         current_index++, current_nonce += batch_size)
    {
        cudaStream_t stream = m_streams[current_index];
        buffer = m_search_buf[current_index];
        // Wait for stream batch to complete
        CUDA_SAFE_CALL(cudaStreamSynchronize(stream));

        // See if we got solutions in this batch
        found_count = buffer->count;
        if (found_count)
        {
            buffer->count = 0;
            uint64_t nonces[SEARCH_RESULTS];
            h256 mixes[SEARCH_RESULTS];
            // handle the highly unlikely possibility that there are more
            // solutions found than we can handle
            if (found_count > SEARCH_RESULTS)
                found_count = SEARCH_RESULTS;
            uint64_t nonce_base = current_nonce - streams_batch_size;
            // stash the solutions, so we can reuse the search buffer
            for (unsigned int j = 0; j < found_count; j++)
            {
                nonces[j] = nonce_base + buffer->result[j].gid;
                memcpy(mixes[j].data(), (void*)&buffer->result[j].mix,
                           sizeof(buffer->result[j].mix));
            }

            // Pass the solutions up to the higher level
            solution = Solution{nonces[0], mixes[0], false};
            return;
        }            
    }

    if (!found_count)
        solution = Solution{current_nonce, h256{}, false};
}

bool CUDAMiner::mine(const WorkPackage &w, Solution &solution)
{
    if (m_currentWP.blockNumber / ETHASH_EPOCH_LENGTH != w.blockNumber / ETHASH_EPOCH_LENGTH)
    {
        if (!init(w.blockNumber))
        {
            return false;
        }
    }

    // Persist most recent job anyway. No need to do another
    // conditional check if they're different
    m_currentWP = w;

    uint64_t upper64OfBoundary = (uint64_t)(u64)((u256)m_currentWP.boundary >> 192);
    // Eventually start searching
    search(m_currentWP.header.data(), upper64OfBoundary, m_currentWP, solution);
    return true;
}
