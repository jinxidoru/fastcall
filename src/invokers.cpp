#include "invokers.h"
#include "asyncresultbase.h"
#include "deps.h"
#include "functionbase.h"
#include "helpers.h"
#include "librarybase.h"
#include "locker.h"
#include "target.h"

using namespace v8;
using namespace node;
using namespace std;
using namespace fastcall;

namespace {
const unsigned syncCallMode = 1;
const unsigned asyncCallMode = 2;

typedef std::function<void(DCCallVM*, const Nan::FunctionCallbackInfo<v8::Value>&)> TSyncVMInitialzer;
typedef std::function<v8::Local<v8::Value>(DCCallVM*)> TSyncVMInvoker;

typedef std::function<TAsyncInvoker(const Nan::FunctionCallbackInfo<v8::Value>&)> TAsyncVMInitialzer;
typedef std::function<TAsyncInvoker()> TAsyncVMInvoker;

#define dcArgInt8 dcArgChar

inline void dcArgUInt8(DCCallVM* vm, unsigned char p)
{
    dcArgChar(vm, reinterpret_cast<char&>(p));
}

#define dcCallInt8 dcCallChar

#define dcArgInt16 dcArgShort

inline void dcArgUInt16(DCCallVM* vm, unsigned short p)
{
    dcArgShort(vm, reinterpret_cast<short&>(p));
}

#define dcCallInt16 dcCallShort

#define dcArgInt32 dcArgInt

inline void dcArgUInt32(DCCallVM* vm, unsigned int p)
{
    dcArgInt(vm, reinterpret_cast<int&>(p));
}

#define dcCallInt32 dcCallInt

#define dcArgInt64 dcArgLongLong

inline void dcArgUInt64(DCCallVM* vm, unsigned long long p)
{
    dcArgLongLong(vm, reinterpret_cast<long long&>(p));
}

#define dcCallInt64 dcCallLongLong

#define dcArgByte dcArgUInt8

inline void dcArgUChar(DCCallVM* vm, unsigned char p)
{
    dcArgChar(vm, reinterpret_cast<char&>(p));
}

inline void dcArgUShort(DCCallVM* vm, unsigned short p)
{
    dcArgShort(vm, reinterpret_cast<short&>(p));
}

inline void dcArgUInt(DCCallVM* vm, unsigned int p)
{
    dcArgInt(vm, reinterpret_cast<int&>(p));
}

inline void dcArgULong(DCCallVM* vm, unsigned long p)
{
    dcArgLong(vm, reinterpret_cast<long&>(p));
}

inline void dcArgULongLong(DCCallVM* vm, unsigned long long p)
{
    dcArgLong(vm, reinterpret_cast<long long&>(p));
}

inline void dcArgSizeT(DCCallVM* vm, size_t p)
{
    auto tmp = static_cast<unsigned long long>(p);
    dcArgLongLong(vm, reinterpret_cast<long long&>(tmp));
}

#define dcArgBool dcArgUInt8

inline uint8_t dcCallUInt8(DCCallVM* vm, void* f)
{
    int8_t tmp = dcCallInt8(vm, f);
    return reinterpret_cast<uint8_t&>(tmp);
}

inline uint16_t dcCallUInt16(DCCallVM* vm, void* f)
{
    int16_t tmp = dcCallInt16(vm, f);
    return reinterpret_cast<uint16_t&>(tmp);
}

inline uint32_t dcCallUInt32(DCCallVM* vm, void* f)
{
    int32_t tmp = dcCallInt32(vm, f);
    return reinterpret_cast<uint32_t&>(tmp);
}

inline uint64_t dcCallUInt64(DCCallVM* vm, void* f)
{
    int64_t tmp = dcCallInt64(vm, f);
    return reinterpret_cast<uint64_t&>(tmp);
}

#define dcCallByte dcCallUInt8

inline unsigned char dcCallUChar(DCCallVM* vm, void* f)
{
    char tmp = dcCallChar(vm, f);
    return reinterpret_cast<unsigned char&>(tmp);
}

inline unsigned short dcCallUShort(DCCallVM* vm, void* f)
{
    short tmp = dcCallShort(vm, f);
    return reinterpret_cast<unsigned short&>(tmp);
}

inline unsigned int dcCallUInt(DCCallVM* vm, void* f)
{
    int tmp = dcCallInt(vm, f);
    return reinterpret_cast<unsigned int&>(tmp);
}

inline unsigned long dcCallULong(DCCallVM* vm, void* f)
{
    long tmp = dcCallLong(vm, f);
    return reinterpret_cast<unsigned long&>(tmp);
}

inline unsigned long long dcCallULongLong(DCCallVM* vm, void* f)
{
    long long tmp = dcCallLongLong(vm, f);
    return reinterpret_cast<unsigned long long&>(tmp);
}

inline size_t dcCallSizeT(DCCallVM* vm, void* f)
{
    long long tmp = dcCallLongLong(vm, f);
    return reinterpret_cast<size_t&>(tmp);
}

inline void* GetPointerAt(const Nan::FunctionCallbackInfo<v8::Value>& info, const unsigned index)
{
    Nan::HandleScope scope;

    auto val = info[index];
    if (val->IsNull() || val->IsUndefined()) {
        return nullptr;
    }
    auto obj = val.As<Object>();
    if (!Buffer::HasInstance(obj)) {
        throw logic_error(string("Argument at index ") + to_string(index) + " is not a pointer.");
    }
    return reinterpret_cast<void*>(Buffer::Data(obj));
}

inline int8_t GetInt8At(const Nan::FunctionCallbackInfo<v8::Value>& info, const unsigned index)
{
    return static_cast<int8_t>(info[index]->Int32Value());
}

inline uint8_t GetUInt8At(const Nan::FunctionCallbackInfo<v8::Value>& info, const unsigned index)
{
    return static_cast<uint8_t>(info[index]->Uint32Value());
}

inline int16_t GetInt16At(const Nan::FunctionCallbackInfo<v8::Value>& info, const unsigned index)
{
    return static_cast<int16_t>(info[index]->Int32Value());
}

inline uint16_t GetUInt16At(const Nan::FunctionCallbackInfo<v8::Value>& info, const unsigned index)
{
    return static_cast<uint16_t>(info[index]->Uint32Value());
}

inline int32_t GetInt32At(const Nan::FunctionCallbackInfo<v8::Value>& info, const unsigned index)
{
    return static_cast<int32_t>(info[index]->Int32Value());
}

inline uint32_t GetUInt32At(const Nan::FunctionCallbackInfo<v8::Value>& info, const unsigned index)
{
    return static_cast<uint32_t>(info[index]->Uint32Value());
}

// TODO: proper 64 bit support, like node-ffi
inline int64_t GetInt64At(const Nan::FunctionCallbackInfo<v8::Value>& info, const unsigned index)
{
    return static_cast<int64_t>(info[index]->NumberValue());
}

// TODO: proper 64 bit support, like node-ffi
inline uint64_t GetUInt64At(const Nan::FunctionCallbackInfo<v8::Value>& info, const unsigned index)
{
    return static_cast<uint64_t>(info[index]->NumberValue());
}

inline float GetFloatAt(const Nan::FunctionCallbackInfo<v8::Value>& info, const unsigned index)
{
    return static_cast<float>(info[index]->NumberValue());
}

inline double GetDoubleAt(const Nan::FunctionCallbackInfo<v8::Value>& info, const unsigned index)
{
    return info[index]->NumberValue();
}

inline char GetCharAt(const Nan::FunctionCallbackInfo<v8::Value>& info, const unsigned index)
{
    return static_cast<char>(GetInt16At(info, index));
}

inline unsigned char GetUCharAt(const Nan::FunctionCallbackInfo<v8::Value>& info, const unsigned index)
{
    return static_cast<unsigned char>(GetUInt16At(info, index));
}

inline uint8_t GetByteAt(const Nan::FunctionCallbackInfo<v8::Value>& info, const unsigned index)
{
    return GetUInt8At(info, index);
}

inline short GetShortAt(const Nan::FunctionCallbackInfo<v8::Value>& info, const unsigned index)
{
    return static_cast<short>(GetInt16At(info, index));
}

inline unsigned short GetUShortAt(const Nan::FunctionCallbackInfo<v8::Value>& info, const unsigned index)
{
    return static_cast<unsigned short>(GetUInt16At(info, index));
}

inline int GetIntAt(const Nan::FunctionCallbackInfo<v8::Value>& info, const unsigned index)
{
    return static_cast<int>(GetInt32At(info, index));
}

inline unsigned int GetUIntAt(const Nan::FunctionCallbackInfo<v8::Value>& info, const unsigned index)
{
    return static_cast<unsigned int>(GetUInt32At(info, index));
}

inline long GetLongAt(const Nan::FunctionCallbackInfo<v8::Value>& info, const unsigned index)
{
    return static_cast<long>(GetInt64At(info, index));
}

inline unsigned long GetULongAt(const Nan::FunctionCallbackInfo<v8::Value>& info, const unsigned index)
{
    return static_cast<unsigned long>(GetUInt64At(info, index));
}

inline long long GetLongLongAt(const Nan::FunctionCallbackInfo<v8::Value>& info, const unsigned index)
{
    return static_cast<long long>(GetInt64At(info, index));
}

inline unsigned long long GetULongLongAt(const Nan::FunctionCallbackInfo<v8::Value>& info, const unsigned index)
{
    return static_cast<unsigned long long>(GetUInt64At(info, index));
}

// TODO: proper 64 bit support, like node-ffi
inline size_t GetSizeTAt(const Nan::FunctionCallbackInfo<v8::Value>& info, const unsigned index)
{
    return static_cast<size_t>(GetUInt64At(info, index));
}

inline bool GetBoolAt(const Nan::FunctionCallbackInfo<v8::Value>& info, const unsigned index)
{
    return info[index]->BooleanValue();
}

inline AsyncResultBase* AsAsyncResultBase(const Nan::FunctionCallbackInfo<v8::Value>& info, const unsigned index)
{
    Nan::HandleScope scope;

    return AsyncResultBase::AsAsyncResultBase(info[index].As<Object>());
}

template <typename T>
inline T* AsAsyncResultPtr(const Nan::FunctionCallbackInfo<v8::Value>& info, const unsigned index)
{
    auto basePtr = AsAsyncResultBase(info, index);
    if (!basePtr) {
        return nullptr;
    }
    return basePtr->GetPtr<T>();
}

inline v8::Local<v8::Object> GetResultPointerType(v8::Local<v8::Object> refType)
{
    Nan::EscapableHandleScope scope;

    auto ref = RequireRef();
    auto derefType = GetValue<Function>(ref, "derefType");
    assert(!derefType.IsEmpty());
    v8::Local<v8::Value> args[] = { refType };
    auto result = Nan::Call(derefType, ref, 1, args).ToLocalChecked().As<Object>();
    assert(!result.IsEmpty());
    return scope.Escape(result);
}

template <typename T, typename F, typename G>
TSyncVMInitialzer MakeSyncArgProcessor(unsigned i, F f, G g)
{
    return [=](DCCallVM* vm, const Nan::FunctionCallbackInfo<v8::Value>& info) {
        T* valPtr = AsAsyncResultPtr<T>(info, i);
        if (valPtr) {
            f(vm, *valPtr);
        }
        else {
            f(vm, g(info, i));
        }
    };
}

TSyncVMInitialzer MakeSyncVMInitializer(const v8::Local<Object>& func)
{
    Nan::HandleScope scope;

    std::vector<TSyncVMInitialzer> list;

    auto args = GetValue<Array>(func, "args");
    unsigned length = args->Length();
    list.reserve(length);
    for (unsigned i = 0; i < length; i++) {
        auto arg = args->Get(i).As<Object>();
        auto type = GetValue<Object>(arg, "type");
        auto typeName = *Nan::Utf8String(GetValue<String>(type, "name"));
        auto indirection = GetValue(type, "indirection")->Uint32Value();
        if (indirection > 1) {
            // pointer:
            list.emplace_back(MakeSyncArgProcessor<void*>(i, dcArgPointer, GetPointerAt));
            continue;
        } else if (indirection == 1) {
            if (!strcmp(typeName, "int8")) {
                list.emplace_back(MakeSyncArgProcessor<int8_t>(i, dcArgInt8, GetInt8At));
                continue;
            }
            if (!strcmp(typeName, "uint8")) {
                list.emplace_back(MakeSyncArgProcessor<uint8_t>(i, dcArgUInt8, GetUInt8At));
                continue;
            }
            if (!strcmp(typeName, "int16")) {
                list.emplace_back(MakeSyncArgProcessor<int16_t>(i, dcArgInt16, GetInt16At));
                continue;
            }
            if (!strcmp(typeName, "uint16")) {
                list.emplace_back(MakeSyncArgProcessor<uint16_t>(i, dcArgUInt16, GetUInt16At));
                continue;
            }
            if (!strcmp(typeName, "int32")) {
                list.emplace_back(MakeSyncArgProcessor<int32_t>(i, dcArgInt32, GetInt32At));
                continue;
            }
            if (!strcmp(typeName, "uint32")) {
                list.emplace_back(MakeSyncArgProcessor<uint32_t>(i, dcArgUInt32, GetUInt32At));
                continue;
            }
            if (!strcmp(typeName, "int64")) {
                list.emplace_back(MakeSyncArgProcessor<int64_t>(i, dcArgInt64, GetInt64At));
                continue;
            }
            if (!strcmp(typeName, "uint64")) {
                list.emplace_back(MakeSyncArgProcessor<uint64_t>(i, dcArgUInt64, GetUInt64At));
                continue;
            }
            if (!strcmp(typeName, "float")) {
                list.emplace_back(MakeSyncArgProcessor<float>(i, dcArgFloat, GetFloatAt));
                continue;
            }
            if (!strcmp(typeName, "double")) {
                list.emplace_back(MakeSyncArgProcessor<double>(i, dcArgDouble, GetDoubleAt));
                continue;
            }
            if (!strcmp(typeName, "char")) {
                list.emplace_back(MakeSyncArgProcessor<char>(i, dcArgChar, GetCharAt));
                continue;
            }
            if (!strcmp(typeName, "byte")) {
                list.emplace_back(MakeSyncArgProcessor<uint8_t>(i, dcArgByte, GetByteAt));
                continue;
            }
            if (!strcmp(typeName, "uchar")) {
                list.emplace_back(MakeSyncArgProcessor<unsigned char>(i, dcArgUChar, GetUCharAt));
                continue;
            }
            if (!strcmp(typeName, "short")) {
                list.emplace_back(MakeSyncArgProcessor<short>(i, dcArgShort, GetShortAt));
                continue;
            }
            if (!strcmp(typeName, "ushort")) {
                list.emplace_back(MakeSyncArgProcessor<unsigned short>(i, dcArgUShort, GetUShortAt));
                continue;
            }
            if (!strcmp(typeName, "int")) {
                list.emplace_back(MakeSyncArgProcessor<int>(i, dcArgInt, GetIntAt));
                continue;
            }
            if (!strcmp(typeName, "uint")) {
                list.emplace_back(MakeSyncArgProcessor<unsigned int>(i, dcArgUInt, GetUIntAt));
                continue;
            }
            if (!strcmp(typeName, "long")) {
                list.emplace_back(MakeSyncArgProcessor<long>(i, dcArgLong, GetLongAt));
                continue;
            }
            if (!strcmp(typeName, "ulong")) {
                list.emplace_back(MakeSyncArgProcessor<unsigned long>(i, dcArgULong, GetULongAt));
                continue;
            }
            // TODO: proper 64 bit support, like node-ffi
            if (!strcmp(typeName, "longlong")) {
                list.emplace_back(MakeSyncArgProcessor<long long>(i, dcArgLongLong, GetLongLongAt));
                continue;
            }
            // TODO: proper 64 bit support, like node-ffi
            if (!strcmp(typeName, "ulonglong")) {
                list.emplace_back(MakeSyncArgProcessor<unsigned long long>(i, dcArgULongLong, GetULongLongAt));
                continue;
            }
            if (!strcmp(typeName, "bool")) {
                list.emplace_back(MakeSyncArgProcessor<bool>(i, dcArgBool, GetBoolAt));
                continue;
            }
            // TODO: proper 64 bit support, like node-ffi
            if (!strcmp(typeName, "size_t")) {
                list.emplace_back(MakeSyncArgProcessor<size_t>(i, dcArgSizeT, GetSizeTAt));
                continue;
            }
        }

        throw logic_error(string("Invalid argument type definition at: ") + to_string(i) + ".");
    }

    return [=](DCCallVM* vm, const Nan::FunctionCallbackInfo<v8::Value>& info) {
        dcReset(vm);
        for (auto& f : list) {
            f(vm, info);
        }
    };
}

template <typename T, typename F, typename G>
TAsyncVMInitialzer MakeAsyncArgProcessor(unsigned i, F f, G g)
{
    return [=](const Nan::FunctionCallbackInfo<v8::Value>& info) {
        auto ar = AsAsyncResultBase(info, i);
        TAsyncInvoker result;
        if (ar) {
            T* valPtr = ar->GetPtr<T>();
            result = [=](DCCallVM* vm)
            {
                f(vm, *valPtr);
            };
        }
        else {
            auto value = g(info, i);
            result = [=](DCCallVM* vm) {
                f(vm, value);
            };
        }
        return result;
    };
}

TAsyncVMInitialzer MakeAsyncVMInitializer(const v8::Local<Object>& func)
{
    Nan::HandleScope scope;

    std::vector<TAsyncVMInitialzer> list;

    auto args = GetValue<Array>(func, "args");
    unsigned length = args->Length();
    list.reserve(length);
    for (unsigned i = 0; i < length; i++) {
        auto arg = args->Get(i).As<Object>();
        auto type = GetValue<Object>(arg, "type");
        auto typeName = *Nan::Utf8String(GetValue<String>(type, "name"));
        auto indirection = GetValue(type, "indirection")->Uint32Value();
        if (indirection > 1) {
            // pointer:
            list.emplace_back(MakeAsyncArgProcessor<void*>(i, dcArgPointer, GetPointerAt));
            continue;
        } else if (indirection == 1) {
            if (!strcmp(typeName, "int8")) {
                list.emplace_back(MakeAsyncArgProcessor<int8_t>(i, dcArgInt8, GetInt8At));
                continue;
            }
            if (!strcmp(typeName, "uint8")) {
                list.emplace_back(MakeAsyncArgProcessor<uint8_t>(i, dcArgUInt8, GetUInt8At));
                continue;
            }
            if (!strcmp(typeName, "int16")) {
                list.emplace_back(MakeAsyncArgProcessor<int16_t>(i, dcArgInt16, GetInt16At));
                continue;
            }
            if (!strcmp(typeName, "uint16")) {
                list.emplace_back(MakeAsyncArgProcessor<uint16_t>(i, dcArgUInt16, GetUInt16At));
                continue;
            }
            if (!strcmp(typeName, "int32")) {
                list.emplace_back(MakeAsyncArgProcessor<int32_t>(i, dcArgInt32, GetInt32At));
                continue;
            }
            if (!strcmp(typeName, "uint32")) {
                list.emplace_back(MakeAsyncArgProcessor<uint32_t>(i, dcArgUInt32, GetUInt32At));
                continue;
            }
            if (!strcmp(typeName, "int64")) {
                list.emplace_back(MakeAsyncArgProcessor<int64_t>(i, dcArgInt64, GetInt64At));
                continue;
            }
            if (!strcmp(typeName, "uint64")) {
                list.emplace_back(MakeAsyncArgProcessor<uint64_t>(i, dcArgUInt64, GetUInt64At));
                continue;
            }
            if (!strcmp(typeName, "float")) {
                list.emplace_back(MakeAsyncArgProcessor<float>(i, dcArgFloat, GetFloatAt));
                continue;
            }
            if (!strcmp(typeName, "double")) {
                list.emplace_back(MakeAsyncArgProcessor<double>(i, dcArgDouble, GetDoubleAt));
                continue;
            }
            if (!strcmp(typeName, "char")) {
                list.emplace_back(MakeAsyncArgProcessor<char>(i, dcArgChar, GetCharAt));
                continue;
            }
            if (!strcmp(typeName, "byte")) {
                list.emplace_back(MakeAsyncArgProcessor<uint8_t>(i, dcArgByte, GetByteAt));
                continue;
            }
            if (!strcmp(typeName, "uchar")) {
                list.emplace_back(MakeAsyncArgProcessor<unsigned char>(i, dcArgUChar, GetUCharAt));
                continue;
            }
            if (!strcmp(typeName, "short")) {
                list.emplace_back(MakeAsyncArgProcessor<short>(i, dcArgShort, GetShortAt));
                continue;
            }
            if (!strcmp(typeName, "ushort")) {
                list.emplace_back(MakeAsyncArgProcessor<unsigned short>(i, dcArgUShort, GetUShortAt));
                continue;
            }
            if (!strcmp(typeName, "int")) {
                list.emplace_back(MakeAsyncArgProcessor<int>(i, dcArgInt, GetIntAt));
                continue;
            }
            if (!strcmp(typeName, "uint")) {
                list.emplace_back(MakeAsyncArgProcessor<unsigned int>(i, dcArgUInt, GetUIntAt));
                continue;
            }
            if (!strcmp(typeName, "long")) {
                list.emplace_back(MakeAsyncArgProcessor<long>(i, dcArgLong, GetLongAt));
                continue;
            }
            if (!strcmp(typeName, "ulong")) {
                list.emplace_back(MakeAsyncArgProcessor<unsigned long>(i, dcArgULong, GetULongAt));
                continue;
            }
            // TODO: proper 64 bit support, like node-ffi
            if (!strcmp(typeName, "longlong")) {
                list.emplace_back(MakeAsyncArgProcessor<long long>(i, dcArgLongLong, GetLongLongAt));
                continue;
            }
            // TODO: proper 64 bit support, like node-ffi
            if (!strcmp(typeName, "ulonglong")) {
                list.emplace_back(MakeAsyncArgProcessor<unsigned long long>(i, dcArgULongLong, GetULongLongAt));
                continue;
            }
            if (!strcmp(typeName, "bool")) {
                list.emplace_back(MakeAsyncArgProcessor<bool>(i, dcArgBool, GetBoolAt));
                continue;
            }
            // TODO: proper 64 bit support, like node-ffi
            if (!strcmp(typeName, "size_t")) {
                list.emplace_back(MakeAsyncArgProcessor<size_t>(i, dcArgSizeT, GetSizeTAt));
                continue;
            }
        }

        throw logic_error(string("Invalid argument type definition at: ") + to_string(i) + ".");
    }

    return [=](const Nan::FunctionCallbackInfo<v8::Value>& info) {
        std::vector<TAsyncInvoker> invokers;
        invokers.reserve(list.size());
        for (auto& f : list) {
            invokers.emplace_back(f(info));
        }

        return bind(
            [=](const std::vector<TAsyncInvoker>& invokers, DCCallVM* vm) {
                dcReset(vm);
                for (auto& f : invokers) {
                    f(vm);
                }
            },
            std::move(invokers),
            std::placeholders::_1);
    };
}

TSyncVMInvoker MakeSyncVMInvoker(const v8::Local<Object>& func)
{
    Nan::HandleScope scope;

    auto resultType = GetValue<Object>(func, "resultType");
    auto resultTypeName = string(*Nan::Utf8String(GetValue<String>(resultType, "name")));
    auto indirection = GetValue(resultType, "indirection")->Uint32Value();
    void* funcPtr = FunctionBase::GetFuncPtr(func);

    assert(GetValue(func, "callMode")->Uint32Value() == 1);

    if (indirection > 1) {
        auto resultPointerType = GetResultPointerType(resultType);
        auto pResultType = Nan::Persistent<Object, CopyablePersistentTraits<Object> >();
        pResultType.Reset(resultPointerType);

        return [=](DCCallVM* vm) {
            Nan::EscapableHandleScope scope;

            void* result = dcCallPointer(vm, funcPtr);
            auto ref = WrapPointer(result);
            SetValue(ref, "type", Nan::New(pResultType));
            return scope.Escape(ref);
        };
    } else if (indirection == 1) {
        auto typeName = resultTypeName.c_str();

        if (!strcmp(typeName, "void")) {
            return [=](DCCallVM* vm) {
                dcCallVoid(vm, funcPtr);
                return Nan::Undefined();
            };
        }
        if (!strcmp(typeName, "int8")) {
            return [=](DCCallVM* vm) {
                int8_t result = dcCallInt8(vm, funcPtr);
                return Nan::New(result);
            };
        }
        if (!strcmp(typeName, "uint8")) {
            return [=](DCCallVM* vm) {
                uint8_t result = dcCallUInt8(vm, funcPtr);
                return Nan::New(result);
            };
        }
        if (!strcmp(typeName, "int16")) {
            return [=](DCCallVM* vm) {
                int16_t result = dcCallInt16(vm, funcPtr);
                return Nan::New(result);
            };
        }
        if (!strcmp(typeName, "uint16")) {
            return [=](DCCallVM* vm) {
                uint16_t result = dcCallUInt16(vm, funcPtr);
                return Nan::New(result);
            };
        }
        if (!strcmp(typeName, "int32")) {
            return [=](DCCallVM* vm) {
                int32_t result = dcCallInt32(vm, funcPtr);
                return Nan::New(result);
            };
        }
        if (!strcmp(typeName, "uint32")) {
            return [=](DCCallVM* vm) {
                uint32_t result = dcCallUInt32(vm, funcPtr);
                return Nan::New(result);
            };
        }
        if (!strcmp(typeName, "int64")) {
            // TODO: proper 64 bit support, like node-ffi
            return [=](DCCallVM* vm) {
                int64_t result = dcCallInt64(vm, funcPtr);
                return Nan::New((double)result);
            };
        }
        if (!strcmp(typeName, "uint64")) {
            // TODO: proper 64 bit support, like node-ffi
            return [=](DCCallVM* vm) {
                uint64_t result = dcCallUInt64(vm, funcPtr);
                return Nan::New((double)result);
            };
        }
        if (!strcmp(typeName, "float")) {
            return [=](DCCallVM* vm) {
                float result = dcCallFloat(vm, funcPtr);
                return Nan::New(result);
            };
        }
        if (!strcmp(typeName, "double")) {
            return [=](DCCallVM* vm) {
                double result = dcCallDouble(vm, funcPtr);
                return Nan::New(result);
            };
        }
        if (!strcmp(typeName, "char")) {
            return [=](DCCallVM* vm) {
                char result = dcCallChar(vm, funcPtr);
                return Nan::New(result);
            };
        }
        if (!strcmp(typeName, "byte")) {
            return [=](DCCallVM* vm) {
                uint8_t result = dcCallUInt8(vm, funcPtr);
                return Nan::New(result);
            };
        }
        if (!strcmp(typeName, "uchar")) {
            return [=](DCCallVM* vm) {
                unsigned char result = dcCallUChar(vm, funcPtr);
                return Nan::New(result);
            };
        }
        if (!strcmp(typeName, "short")) {
            return [=](DCCallVM* vm) {
                short result = dcCallShort(vm, funcPtr);
                return Nan::New(result);
            };
        }
        if (!strcmp(typeName, "ushort")) {
            return [=](DCCallVM* vm) {
                unsigned short result = dcCallUShort(vm, funcPtr);
                return Nan::New(result);
            };
        }
        if (!strcmp(typeName, "int")) {
            return [=](DCCallVM* vm) {
                int result = dcCallInt(vm, funcPtr);
                return Nan::New(result);
            };
        }
        if (!strcmp(typeName, "uint")) {
            return [=](DCCallVM* vm) {
                unsigned int result = dcCallUInt(vm, funcPtr);
                return Nan::New(result);
            };
        }
        if (!strcmp(typeName, "long")) {
            return [=](DCCallVM* vm) {
                long result = dcCallLong(vm, funcPtr);
                return Nan::New((double)result);
            };
        }
        if (!strcmp(typeName, "ulong")) {
            return [=](DCCallVM* vm) {
                unsigned long result = dcCallULong(vm, funcPtr);
                return Nan::New((double)result);
            };
        }
        // TODO: proper 64 bit support, like node-ffi
        if (!strcmp(typeName, "longlong")) {
            return [=](DCCallVM* vm) {
                long long result = dcCallLongLong(vm, funcPtr);
                return Nan::New((double)result);
            };
        }
        // TODO: proper 64 bit support, like node-ffi
        if (!strcmp(typeName, "ulonglong")) {
            return [=](DCCallVM* vm) {
                unsigned long long result = dcCallULongLong(vm, funcPtr);
                return Nan::New((double)result);
            };
        }
        if (!strcmp(typeName, "bool")) {
            return [=](DCCallVM* vm) {
                bool result = dcCallBool(vm, funcPtr) != 0;
                return Nan::New(result);
            };
        }
        // TODO: proper 64 bit support, like node-ffi
        if (!strcmp(typeName, "size_t")) {
            return [=](DCCallVM* vm) {
                size_t result = dcCallSizeT(vm, funcPtr);
                return Nan::New((double)result);
            };
        }
    }
    throw logic_error("Invalid resultType.");
}
}

TInvoker fastcall::MakeInvoker(const v8::Local<Object>& func)
{
    unsigned callMode = GetValue(func, "callMode")->Uint32Value();
    auto funcBase = FunctionBase::GetFunctionBase(func);

    if (callMode == syncCallMode) {
        auto initializer = MakeSyncVMInitializer(func);
        auto invoker = MakeSyncVMInvoker(func);
        return [=](const Nan::FunctionCallbackInfo<v8::Value>& info) {
            auto lock(funcBase->GetLibrary()->AcquireLock());
            initializer(funcBase->GetVM(), info);
            return invoker(funcBase->GetVM());
        };
    } else if (callMode == asyncCallMode) {
        // Note: this branch's invocation and stuff gets locked in the Loop.
        funcBase->GetLibrary()->EnsureAsyncSupport();
        assert(false);
    } else {
        assert(false);
    }
}