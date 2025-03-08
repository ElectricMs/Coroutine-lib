#include "thread.h"

#include <sys/syscall.h> //一个Unix/Linux系统特有的头文件，用于直接进行系统调用
#include <iostream>
#include <unistd.h>  

namespace sylar {

// 线程信息
static thread_local Thread* t_thread          = nullptr;
static thread_local std::string t_thread_name = "UNKNOWN";
/* 
thread_local 用于声明线程局部存储（Thread Local Storage, TLS）的变量。
每个线程都有自己独立的实例，这意味着如果一个变量被声明为 thread_local，
那么每个线程访问这个变量时都会得到自己的私有副本，而不是共享同一个实例。
*/

//pid_t 是一个数据类型，通常在 <sys/types.h> 或者其他相关头文件（如你代码中的 <sys/syscall.h>）中定义。它是一个整数类型，用于表示进程ID（PID）
//Thread::使用类名作为限定符来明确指出该函数属于哪个类，用于静态成员函数
pid_t Thread::GetThreadId()
{
	return syscall(SYS_gettid);
}

Thread* Thread::GetThis()
{
    return t_thread;
}

//在成员函数声明中的 const 表示这个函数不会修改类的任何成员变量（除了那些被声明为 mutable 的成员）
//这有助于保证函数的安全性，并允许它被常量对象调用
/*
返回类型是 const std::string&，即返回的是一个对 std::string 类型的常量引用。
使用引用返回值可以避免不必要的拷贝操作，提高效率。
如果直接返回 std::string 而不是引用，则每次调用都会创建一个新的字符串副本。
加上 const 修饰符意味着返回的引用不能用来修改原字符串。
这样做的好处是可以保护内部状态不被意外修改，同时还能提供高效的访问方式。
*/
const std::string& Thread::GetName() 
{
    return t_thread_name;
}

void Thread::SetName(const std::string &name) 
{
    if (t_thread) 
    {
        t_thread->m_name = name;
    }
    t_thread_name = name;
}

Thread::Thread(std::function<void()> cb, const std::string &name): 
m_cb(cb), m_name(name) 
{
    //使用 pthread_create 创建一个新的线程，并传入静态成员函数 run 作为入口点
    int rt = pthread_create(&m_thread, nullptr, &Thread::run, this);
    if (rt) 
    {
        std::cerr << "pthread_create thread fail, rt=" << rt << " name=" << name;
        throw std::logic_error("pthread_create error");
    }
    // 等待线程函数完成初始化
    m_semaphore.wait();
}

Thread::~Thread() 
{
    if (m_thread) 
    {
        //如果线程对象仍然有效，则调用 pthread_detach 来分离线程，防止僵尸线程的存在
        pthread_detach(m_thread);
        m_thread = 0;
    }
}

void Thread::join() 
{
    if (m_thread) 
    {
        int rt = pthread_join(m_thread, nullptr);
        if (rt) 
        {
            std::cerr << "pthread_join failed, rt = " << rt << ", name = " << m_name << std::endl;
            throw std::logic_error("pthread_join error");
        }
        m_thread = 0;
    }
}

void* Thread::run(void* arg) 
{
    Thread* thread = (Thread*)arg;

    t_thread       = thread;
    t_thread_name  = thread->m_name;
    thread->m_id   = GetThreadId();
    //设置线程名称（最多15个字符）
    pthread_setname_np(pthread_self(), thread->m_name.substr(0, 15).c_str());

    //使用 swap 方法交换 m_cb 和一个临时的 std::function<void()> 对象，这有助于减少智能指针的引用计数
    std::function<void()> cb;
    cb.swap(thread->m_cb); // swap -> 可以减少m_cb中只能指针的引用计数
    
    // 初始化完成
    thread->m_semaphore.signal();

    cb();
    return 0;
}

} 
