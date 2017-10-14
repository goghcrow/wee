#ifndef DUBBO_CODEC_H
#define DUBBO_CODEC_H

#include <stdbool.h>
#include "../base/buffer.h"

// dubbo 泛化调用 codec

/* dubbo 泛化调用
public interface GenericService {
    **
     * 泛化调用 (默认实现, 不支持)
     *
     * @param method 方法名，如：findPerson，如果有重载方法，需带上参数列表，如：findPerson(java.lang.String)
     * @param parameterTypes 参数类型
     * @param args 参数列表
     * @return 返回值
     * @throws Throwable 方法抛出的异常
     *
    Object $invoke(String method, String[] parameterTypes, Object[] args) throws GenericException;

    **
     * (支持)
     * 泛化调用，方法参数和返回结果使用json序列化，方法参数的key从arg0开始
     * @param method
     * @param parameterTypes
     * @param jsonArgs
     * @return
     * @throws GenericException
     *
    String $invokeWithJsonArgs(String method, String[] parameterTypes, String jsonArgs) throws GenericException;
}
*/


struct dubbo_req;
struct dubbo_res
{
};

struct dubbo_req *dubbo_req_create();
void dubbo_req_release(struct dubbo_req *);

struct buffer* dubbo_encode(struct dubbo_req *);
bool dubbo_decode(struct buffer *, struct dubbo_res *);

bool is_dubbo_packet(struct buffer *);

#endif