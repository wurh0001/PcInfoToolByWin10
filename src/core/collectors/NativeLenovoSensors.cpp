#include "core/collectors/NativeLenovoSensors.h"
#pragma comment(lib,"wbemuuid.lib")
#pragma comment(lib,"oleaut32.lib")

namespace {
struct Bstr { BSTR value; explicit Bstr(const wchar_t* text):value(SysAllocString(text)){} ~Bstr(){SysFreeString(value);} };
}
NativeLenovoSensors::NativeLenovoSensors() {
    comStatus=CoInitializeEx(nullptr,COINIT_MULTITHREADED);comOwned=SUCCEEDED(comStatus);
}
NativeLenovoSensors::~NativeLenovoSensors() {
    services.Reset();if(comOwned)CoUninitialize();
}
HRESULT NativeLenovoSensors::Connect() {
    if(FAILED(comStatus))return comStatus;
    Microsoft::WRL::ComPtr<IWbemLocator> locator;
    HRESULT hr=CoCreateInstance(CLSID_WbemLocator,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&locator));
    if(FAILED(hr))return hr;
    Bstr ns(L"ROOT\\WMI");
    hr=locator->ConnectServer(ns.value,nullptr,nullptr,nullptr,WBEM_FLAG_CONNECT_USE_MAX_WAIT,nullptr,nullptr,&services);
    if(FAILED(hr))return hr;
    hr=CoSetProxyBlanket(services.Get(),RPC_C_AUTHN_WINNT,RPC_C_AUTHZ_NONE,nullptr,RPC_C_AUTHN_LEVEL_CALL,RPC_C_IMP_LEVEL_IMPERSONATE,nullptr,EOAC_NONE);
    if(FAILED(hr)){services.Reset();return hr;}
    Bstr language(L"WQL"),query(L"SELECT * FROM LENOVO_GAMEZONE_DATA");
    Microsoft::WRL::ComPtr<IEnumWbemClassObject> enumerator;
    hr=services->ExecQuery(language.value,query.value,WBEM_FLAG_FORWARD_ONLY|WBEM_FLAG_RETURN_IMMEDIATELY,nullptr,&enumerator);
    if(FAILED(hr)){services.Reset();return hr;}
    Microsoft::WRL::ComPtr<IWbemClassObject> instance;ULONG count=0;
    hr=enumerator->Next(1000,1,&instance,&count);
    if(FAILED(hr)||!count){services.Reset();return FAILED(hr)?hr:WBEM_E_NOT_FOUND;}
    VARIANT path;VariantInit(&path);hr=instance->Get(L"__PATH",0,&path,nullptr,nullptr);
    if(SUCCEEDED(hr)&&path.vt==VT_BSTR)devicePath=path.bstrVal;else hr=WBEM_E_INVALID_OBJECT;
    VariantClear(&path);if(FAILED(hr))services.Reset();return hr;
}
HRESULT NativeLenovoSensors::Read(const wchar_t* method,uint32_t& value) {
    Bstr path(devicePath.c_str()),name(method);
    Microsoft::WRL::ComPtr<IWbemCallResult> call;
    HRESULT hr=services->ExecMethod(path.value,name.value,WBEM_FLAG_RETURN_IMMEDIATELY,nullptr,nullptr,nullptr,&call);
    if(FAILED(hr))return hr;
    Microsoft::WRL::ComPtr<IWbemClassObject> result;
    hr=call->GetResultObject(1000,&result);if(hr!=WBEM_S_NO_ERROR||!result)return FAILED(hr)?hr:HRESULT_FROM_WIN32(ERROR_TIMEOUT);
    VARIANT status;VariantInit(&status);
    const HRESULT statusHr=result->Get(L"ReturnValue",0,&status,nullptr,nullptr);
    // CIM exposes boolean success. The classic WMI bridge may omit ReturnValue;
    // in that case a successfully returned, correctly typed Data property is required.
    const bool success=statusHr==WBEM_E_NOT_FOUND||(SUCCEEDED(statusHr)&&((status.vt==VT_BOOL&&status.boolVal!=VARIANT_FALSE)||((status.vt==VT_I4||status.vt==VT_UI4)&&status.ulVal==1)));
    VariantClear(&status);if(!success)return E_FAIL;
    VARIANT data;VariantInit(&data);hr=result->Get(L"Data",0,&data,nullptr,nullptr);
    if(SUCCEEDED(hr)&&(data.vt==VT_UI4||data.vt==VT_I4)&&data.lVal>=0)value=data.ulVal;else hr=WBEM_E_TYPE_MISMATCH;
    VariantClear(&data);return hr;
}
NativeLenovoSensors::Reading NativeLenovoSensors::Sample() {
    const auto now=GetTickCount64();if(now<nextAttempt)return cached;
    Reading r;HRESULT hr=services?S_OK:Connect();
    if(SUCCEEDED(hr)) {
        uint32_t count=0;hr=Read(L"GetFanCount",count);
        if(SUCCEEDED(hr)&&count>0&&count<=8) {
            r.fanCount=static_cast<int>(count);uint32_t rpm=0;
            if(SUCCEEDED(Read(L"GetFan1Speed",rpm))&&rpm<=20000)r.fan1=static_cast<int>(rpm);
            if(count>=2&&SUCCEEDED(Read(L"GetFan2Speed",rpm))&&rpm<=20000)r.fan2=static_cast<int>(rpm);
        }
        uint32_t temperature=0;
        if(SUCCEEDED(Read(L"GetCPUTemp",temperature))&&temperature>0&&temperature<=150)r.cpuTemperature=static_cast<float>(temperature);
    }
    r.status=hr;r.sampleTime=GetTickCount64();cached=r;nextAttempt=r.sampleTime+(FAILED(hr)?30000:2000);
    if(FAILED(hr)){services.Reset();devicePath.clear();}
    return cached;
}
