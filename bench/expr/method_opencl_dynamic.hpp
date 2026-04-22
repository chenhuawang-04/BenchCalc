#pragma once

#include "method_interface.hpp"

#ifdef _WIN32
namespace exprbench {

// ------------------------------ OpenCL minimal loader ------------------------------
using cl_int = int32_t;
using cl_uint = uint32_t;
using cl_ulong = uint64_t;
using cl_bool = cl_uint;
using cl_bitfield = cl_ulong;
using cl_device_type = cl_bitfield;
using cl_platform_info = cl_uint;
using cl_device_info = cl_uint;
using cl_mem_flags = cl_bitfield;
using cl_command_queue_properties = cl_bitfield;
using cl_program_build_info = cl_uint;
using cl_context_properties = intptr_t;

struct _cl_platform_id;
struct _cl_device_id;
struct _cl_context;
struct _cl_command_queue;
struct _cl_mem;
struct _cl_program;
struct _cl_kernel;
struct _cl_event;

using cl_platform_id = _cl_platform_id*;
using cl_device_id = _cl_device_id*;
using cl_context = _cl_context*;
using cl_command_queue = _cl_command_queue*;
using cl_mem = _cl_mem*;
using cl_program = _cl_program*;
using cl_kernel = _cl_kernel*;
using cl_event = _cl_event*;

constexpr cl_int CL_SUCCESS = 0;
constexpr cl_int CL_DEVICE_NOT_FOUND = -1;
constexpr cl_uint CL_TRUE = 1;
constexpr cl_device_type CL_DEVICE_TYPE_GPU = (1ULL << 2);
constexpr cl_context_properties CL_CONTEXT_PLATFORM = 0x1084;
constexpr cl_mem_flags CL_MEM_READ_ONLY = (1ULL << 2);
constexpr cl_mem_flags CL_MEM_WRITE_ONLY = (1ULL << 1);
constexpr cl_program_build_info CL_PROGRAM_BUILD_LOG = 0x1183;
constexpr cl_device_info CL_DEVICE_EXTENSIONS = 0x1030;

#define CL_API_CALL __stdcall

using PFN_clGetPlatformIDs = cl_int(CL_API_CALL*)(cl_uint, cl_platform_id*, cl_uint*);
using PFN_clGetDeviceIDs = cl_int(CL_API_CALL*)(cl_platform_id, cl_device_type, cl_uint, cl_device_id*, cl_uint*);
using PFN_clGetDeviceInfo = cl_int(CL_API_CALL*)(cl_device_id, cl_device_info, size_t, void*, size_t*);
using PFN_clCreateContext = cl_context(CL_API_CALL*)(
    const cl_context_properties*, cl_uint, const cl_device_id*,
    void(CL_API_CALL*)(const char*, const void*, size_t, void*), void*, cl_int*);
using PFN_clCreateCommandQueue = cl_command_queue(CL_API_CALL*)(cl_context, cl_device_id, cl_command_queue_properties, cl_int*);
using PFN_clCreateProgramWithSource = cl_program(CL_API_CALL*)(cl_context, cl_uint, const char**, const size_t*, cl_int*);
using PFN_clBuildProgram = cl_int(CL_API_CALL*)(cl_program, cl_uint, const cl_device_id*, const char*, void(CL_API_CALL*)(cl_program, void*), void*);
using PFN_clGetProgramBuildInfo = cl_int(CL_API_CALL*)(cl_program, cl_device_id, cl_program_build_info, size_t, void*, size_t*);
using PFN_clCreateKernel = cl_kernel(CL_API_CALL*)(cl_program, const char*, cl_int*);
using PFN_clCreateBuffer = cl_mem(CL_API_CALL*)(cl_context, cl_mem_flags, size_t, void*, cl_int*);
using PFN_clSetKernelArg = cl_int(CL_API_CALL*)(cl_kernel, cl_uint, size_t, const void*);
using PFN_clEnqueueWriteBuffer = cl_int(CL_API_CALL*)(cl_command_queue, cl_mem, cl_bool, size_t, size_t, const void*, cl_uint, const cl_event*, cl_event*);
using PFN_clEnqueueNDRangeKernel = cl_int(CL_API_CALL*)(cl_command_queue, cl_kernel, cl_uint, const size_t*, const size_t*, const size_t*, cl_uint, const cl_event*, cl_event*);
using PFN_clEnqueueReadBuffer = cl_int(CL_API_CALL*)(cl_command_queue, cl_mem, cl_bool, size_t, size_t, void*, cl_uint, const cl_event*, cl_event*);
using PFN_clFinish = cl_int(CL_API_CALL*)(cl_command_queue);
using PFN_clReleaseMemObject = cl_int(CL_API_CALL*)(cl_mem);
using PFN_clReleaseKernel = cl_int(CL_API_CALL*)(cl_kernel);
using PFN_clReleaseProgram = cl_int(CL_API_CALL*)(cl_program);
using PFN_clReleaseCommandQueue = cl_int(CL_API_CALL*)(cl_command_queue);
using PFN_clReleaseContext = cl_int(CL_API_CALL*)(cl_context);

struct OpenCLFns {
    HMODULE lib = nullptr;
    PFN_clGetPlatformIDs clGetPlatformIDs = nullptr;
    PFN_clGetDeviceIDs clGetDeviceIDs = nullptr;
    PFN_clGetDeviceInfo clGetDeviceInfo = nullptr;
    PFN_clCreateContext clCreateContext = nullptr;
    PFN_clCreateCommandQueue clCreateCommandQueue = nullptr;
    PFN_clCreateProgramWithSource clCreateProgramWithSource = nullptr;
    PFN_clBuildProgram clBuildProgram = nullptr;
    PFN_clGetProgramBuildInfo clGetProgramBuildInfo = nullptr;
    PFN_clCreateKernel clCreateKernel = nullptr;
    PFN_clCreateBuffer clCreateBuffer = nullptr;
    PFN_clSetKernelArg clSetKernelArg = nullptr;
    PFN_clEnqueueWriteBuffer clEnqueueWriteBuffer = nullptr;
    PFN_clEnqueueNDRangeKernel clEnqueueNDRangeKernel = nullptr;
    PFN_clEnqueueReadBuffer clEnqueueReadBuffer = nullptr;
    PFN_clFinish clFinish = nullptr;
    PFN_clReleaseMemObject clReleaseMemObject = nullptr;
    PFN_clReleaseKernel clReleaseKernel = nullptr;
    PFN_clReleaseProgram clReleaseProgram = nullptr;
    PFN_clReleaseCommandQueue clReleaseCommandQueue = nullptr;
    PFN_clReleaseContext clReleaseContext = nullptr;
};

inline bool load_opencl_fns(OpenCLFns& fns, std::string& reason) {
    fns.lib = LoadLibraryA("OpenCL.dll");
    if (!fns.lib) {
        reason = "OpenCL.dll not found";
        return false;
    }
    auto load = [&](auto& out, const char* name) {
        out = reinterpret_cast<std::remove_reference_t<decltype(out)>>(GetProcAddress(fns.lib, name));
        return out != nullptr;
    };
    bool ok = true;
    ok = ok && load(fns.clGetPlatformIDs, "clGetPlatformIDs");
    ok = ok && load(fns.clGetDeviceIDs, "clGetDeviceIDs");
    ok = ok && load(fns.clGetDeviceInfo, "clGetDeviceInfo");
    ok = ok && load(fns.clCreateContext, "clCreateContext");
    ok = ok && load(fns.clCreateCommandQueue, "clCreateCommandQueue");
    ok = ok && load(fns.clCreateProgramWithSource, "clCreateProgramWithSource");
    ok = ok && load(fns.clBuildProgram, "clBuildProgram");
    ok = ok && load(fns.clGetProgramBuildInfo, "clGetProgramBuildInfo");
    ok = ok && load(fns.clCreateKernel, "clCreateKernel");
    ok = ok && load(fns.clCreateBuffer, "clCreateBuffer");
    ok = ok && load(fns.clSetKernelArg, "clSetKernelArg");
    ok = ok && load(fns.clEnqueueWriteBuffer, "clEnqueueWriteBuffer");
    ok = ok && load(fns.clEnqueueNDRangeKernel, "clEnqueueNDRangeKernel");
    ok = ok && load(fns.clEnqueueReadBuffer, "clEnqueueReadBuffer");
    ok = ok && load(fns.clFinish, "clFinish");
    ok = ok && load(fns.clReleaseMemObject, "clReleaseMemObject");
    ok = ok && load(fns.clReleaseKernel, "clReleaseKernel");
    ok = ok && load(fns.clReleaseProgram, "clReleaseProgram");
    ok = ok && load(fns.clReleaseCommandQueue, "clReleaseCommandQueue");
    ok = ok && load(fns.clReleaseContext, "clReleaseContext");
    if (!ok) {
        reason = "OpenCL symbols missing";
        FreeLibrary(fns.lib);
        fns.lib = nullptr;
        return false;
    }
    return true;
}

inline std::string program_build_log(OpenCLFns& fns, cl_program prog, cl_device_id dev) {
    size_t sz = 0;
    fns.clGetProgramBuildInfo(prog, dev, CL_PROGRAM_BUILD_LOG, 0, nullptr, &sz);
    std::string log(sz, '\0');
    if (sz > 0) fns.clGetProgramBuildInfo(prog, dev, CL_PROGRAM_BUILD_LOG, sz, log.data(), nullptr);
    return log;
}

inline std::string read_device_info_str(OpenCLFns& fns, cl_device_id dev, cl_device_info param) {
    size_t sz = 0;
    if (fns.clGetDeviceInfo(dev, param, 0, nullptr, &sz) != CL_SUCCESS || sz == 0) return {};
    std::string out(sz, '\0');
    if (fns.clGetDeviceInfo(dev, param, sz, out.data(), nullptr) != CL_SUCCESS) return {};
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

inline std::string node_to_c_expr(const Program& prog, int id, const std::vector<std::string>& var_names) {
    const Node& n = prog.nodes[id];
    switch (n.kind) {
    case NodeKind::Var: return var_names[static_cast<size_t>(n.var_index)] + "[gid]";
    case NodeKind::Const: {
        std::ostringstream oss;
        oss << std::setprecision(17) << n.const_value;
        return "(" + oss.str() + ")";
    }
    case NodeKind::Neg: return "(-" + node_to_c_expr(prog, n.lhs, var_names) + ")";
    case NodeKind::Add:
        return "(" + node_to_c_expr(prog, n.lhs, var_names) + " + " + node_to_c_expr(prog, n.rhs, var_names) + ")";
    case NodeKind::Sub:
        return "(" + node_to_c_expr(prog, n.lhs, var_names) + " - " + node_to_c_expr(prog, n.rhs, var_names) + ")";
    case NodeKind::Mul:
        return "(" + node_to_c_expr(prog, n.lhs, var_names) + " * " + node_to_c_expr(prog, n.rhs, var_names) + ")";
    case NodeKind::Div:
        return "(" + node_to_c_expr(prog, n.lhs, var_names) + " / " + node_to_c_expr(prog, n.rhs, var_names) + ")";
    default: return "0";
    }
}

template <typename T>
class OpenCLDynamicKernelMethod final : public IMethod<T> {
public:
    explicit OpenCLDynamicKernelMethod(bool readback_each_timed)
        : readback_each_timed_(readback_each_timed) {}

    std::string name() const override {
        return readback_each_timed_ ? "gpu_dynamic_kernel_e2e" : "gpu_dynamic_kernel_peak";
    }

    bool prepare(const Program& prog,
                 const std::vector<std::vector<T>>& inputs,
                 size_t n,
                 int /*threads*/) override {
        prog_ = &prog;
        n_ = n;
        output_host_.assign(n_, T{});
        avail_ = false;
        reason_.clear();

        if (!load_opencl_fns(fns_, reason_)) return false;
        cl_uint np = 0;
        if (fns_.clGetPlatformIDs(0, nullptr, &np) != CL_SUCCESS || np == 0) {
            reason_ = "no OpenCL platform";
            return false;
        }
        std::vector<cl_platform_id> plats(np);
        if (fns_.clGetPlatformIDs(np, plats.data(), nullptr) != CL_SUCCESS) {
            reason_ = "clGetPlatformIDs failed";
            return false;
        }
        for (cl_platform_id p : plats) {
            cl_uint nd = 0;
            cl_int r = fns_.clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, 0, nullptr, &nd);
            if (r == CL_DEVICE_NOT_FOUND || nd == 0) continue;
            if (r != CL_SUCCESS) continue;
            std::vector<cl_device_id> devs(nd);
            if (fns_.clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, nd, devs.data(), nullptr) != CL_SUCCESS) continue;
            platform_ = p;
            device_ = devs[0];
            break;
        }
        if (!device_) {
            reason_ = "no GPU OpenCL device";
            return false;
        }

        if constexpr (std::is_same_v<T, double>) {
            const std::string ext = read_device_info_str(fns_, device_, CL_DEVICE_EXTENSIONS);
            if (ext.find("cl_khr_fp64") == std::string::npos) {
                reason_ = "device missing cl_khr_fp64";
                return false;
            }
        }

        cl_int err = CL_SUCCESS;
        cl_context_properties props[] = {CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(platform_), 0};
        context_ = fns_.clCreateContext(props, 1, &device_, nullptr, nullptr, &err);
        if (err != CL_SUCCESS || !context_) {
            reason_ = "clCreateContext failed";
            return false;
        }
        queue_ = fns_.clCreateCommandQueue(context_, device_, 0, &err);
        if (err != CL_SUCCESS || !queue_) {
            reason_ = "clCreateCommandQueue failed";
            return false;
        }

        std::vector<std::string> arg_names;
        arg_names.reserve(prog.variables.size());
        for (size_t i = 0; i < prog.variables.size(); ++i) arg_names.push_back("v" + std::to_string(i));
        const std::string expr_c = node_to_c_expr(prog, prog.root, arg_names);

        std::ostringstream src;
        if constexpr (std::is_same_v<T, double>) {
            src << "#pragma OPENCL EXTENSION cl_khr_fp64 : enable\n";
            src << "typedef double scalar_t;\n";
        } else {
            src << "typedef float scalar_t;\n";
        }
        src << "__kernel void eval_expr(";
        for (size_t i = 0; i < prog.variables.size(); ++i) src << "__global const scalar_t* " << arg_names[i] << ", ";
        src << "__global scalar_t* out, int n) {\n";
        src << "  int gid = get_global_id(0);\n";
        src << "  if (gid < n) out[gid] = " << expr_c << ";\n";
        src << "}\n";
        source_ = src.str();

        const char* sp = source_.c_str();
        size_t sl = source_.size();
        program_cl_ = fns_.clCreateProgramWithSource(context_, 1, &sp, &sl, &err);
        if (err != CL_SUCCESS || !program_cl_) {
            reason_ = "clCreateProgramWithSource failed";
            return false;
        }
        err = fns_.clBuildProgram(program_cl_, 1, &device_, "", nullptr, nullptr);
        if (err != CL_SUCCESS) {
            reason_ = "clBuildProgram failed: " + program_build_log(fns_, program_cl_, device_);
            return false;
        }
        kernel_ = fns_.clCreateKernel(program_cl_, "eval_expr", &err);
        if (err != CL_SUCCESS || !kernel_) {
            reason_ = "clCreateKernel failed";
            return false;
        }

        const size_t bytes = n_ * sizeof(T);
        for (size_t v = 0; v < inputs.size(); ++v) {
            cl_mem m = fns_.clCreateBuffer(context_, CL_MEM_READ_ONLY, bytes, nullptr, &err);
            if (err != CL_SUCCESS || !m) {
                reason_ = "clCreateBuffer input failed";
                return false;
            }
            if (fns_.clEnqueueWriteBuffer(queue_, m, CL_TRUE, 0, bytes, inputs[v].data(), 0, nullptr, nullptr) != CL_SUCCESS) {
                reason_ = "clEnqueueWriteBuffer input failed";
                return false;
            }
            input_bufs_.push_back(m);
        }
        output_buf_ = fns_.clCreateBuffer(context_, CL_MEM_WRITE_ONLY, bytes, nullptr, &err);
        if (err != CL_SUCCESS || !output_buf_) {
            reason_ = "clCreateBuffer output failed";
            return false;
        }
        for (size_t i = 0; i < input_bufs_.size(); ++i) {
            if (fns_.clSetKernelArg(kernel_, static_cast<cl_uint>(i), sizeof(cl_mem), &input_bufs_[i]) != CL_SUCCESS) {
                reason_ = "clSetKernelArg input failed";
                return false;
            }
        }
        if (fns_.clSetKernelArg(kernel_, static_cast<cl_uint>(input_bufs_.size()), sizeof(cl_mem), &output_buf_) != CL_SUCCESS) {
            reason_ = "clSetKernelArg output failed";
            return false;
        }
        int n_arg = static_cast<int>(n_);
        if (fns_.clSetKernelArg(kernel_, static_cast<cl_uint>(input_bufs_.size() + 1), sizeof(int), &n_arg) != CL_SUCCESS) {
            reason_ = "clSetKernelArg n failed";
            return false;
        }

        avail_ = true;
        return true;
    }

    bool available() const override { return avail_; }
    std::string unavailable_reason() const override { return reason_; }
    void run_validate(std::vector<T>& out) override {
        run_kernel(true);
        out = output_host_;
    }
    double run_timed() override {
        run_kernel(readback_each_timed_);
        if (readback_each_timed_) return sample_guard(output_host_);
        return 0.0;
    }

    ~OpenCLDynamicKernelMethod() override { release_all(); }

private:
    static double sample_guard(const std::vector<T>& v) {
        if (v.empty()) return 0.0;
        size_t step = std::max<size_t>(1, v.size() / 32);
        double g = 0.0;
        for (size_t i = 0; i < v.size(); i += step) g += static_cast<double>(v[i]);
        return g;
    }
    void run_kernel(bool readback) {
        if (!avail_) return;
        const size_t gws = ((n_ + 255) / 256) * 256;
        const size_t lws = 256;
        fns_.clEnqueueNDRangeKernel(queue_, kernel_, 1, nullptr, &gws, &lws, 0, nullptr, nullptr);
        fns_.clFinish(queue_);
        if (readback) {
            fns_.clEnqueueReadBuffer(queue_, output_buf_, CL_TRUE, 0, n_ * sizeof(T), output_host_.data(), 0, nullptr, nullptr);
        }
    }
    void release_all() {
        for (cl_mem m : input_bufs_) {
            if (m && fns_.clReleaseMemObject) fns_.clReleaseMemObject(m);
        }
        input_bufs_.clear();
        if (output_buf_ && fns_.clReleaseMemObject) fns_.clReleaseMemObject(output_buf_);
        output_buf_ = nullptr;
        if (kernel_ && fns_.clReleaseKernel) fns_.clReleaseKernel(kernel_);
        kernel_ = nullptr;
        if (program_cl_ && fns_.clReleaseProgram) fns_.clReleaseProgram(program_cl_);
        program_cl_ = nullptr;
        if (queue_ && fns_.clReleaseCommandQueue) fns_.clReleaseCommandQueue(queue_);
        queue_ = nullptr;
        if (context_ && fns_.clReleaseContext) fns_.clReleaseContext(context_);
        context_ = nullptr;
        if (fns_.lib) {
            FreeLibrary(fns_.lib);
            fns_.lib = nullptr;
        }
    }

private:
    bool readback_each_timed_ = false;
    const Program* prog_ = nullptr;
    size_t n_ = 0;
    bool avail_ = false;
    std::string reason_;
    std::string source_;
    OpenCLFns fns_;
    cl_platform_id platform_ = nullptr;
    cl_device_id device_ = nullptr;
    cl_context context_ = nullptr;
    cl_command_queue queue_ = nullptr;
    cl_program program_cl_ = nullptr;
    cl_kernel kernel_ = nullptr;
    std::vector<cl_mem> input_bufs_;
    cl_mem output_buf_ = nullptr;
    std::vector<T> output_host_;
};

} // namespace exprbench
#endif
