

#ifndef TYPE_NAME_HH
#define TYPE_NAME_HH

#include <type_traits>
#include <typeinfo>
#ifndef _MSC_VER
#   include <cxxabi.h>
#endif
#include <memory>
#include <string>
#include <cstdlib>


namespace Logical
{

using std::string;
using std::unique_ptr;


template<typename... Args>
string string_format(const string& format, Args&&... args)
{
    size_t size = snprintf(nullptr, 0, format.c_str(), args...) + 1;
    unique_ptr<char[]> buf(new char[size]); 
    snprintf(buf.get(), size, format.c_str(), forward<Args>(args)...);
    return string(buf.get(), buf.get() + size - 1);
}


template<class T>
string type_name(void)
{
    typedef typename std::remove_reference<T>::type TR;
    std::unique_ptr<char, void(*)(void*)> own
           (
#ifndef _MSC_VER
                abi::__cxa_demangle(typeid(TR).name(), nullptr,
                                           nullptr, nullptr),
#else
                nullptr,
#endif
                std::free
           );
    std::string r = own != nullptr ? own.get() : typeid(TR).name();
    if (std::is_const<TR>::value)
        r += " const";
    if (std::is_volatile<TR>::value)
        r += " volatile";
    if (std::is_lvalue_reference<T>::value)
        r += "&";
    else if (std::is_rvalue_reference<T>::value)
        r += "&&";
    return r;
}


} // namespace Logical


#endif // ifndef TYPE_NAME_HH
