#ifndef SBK_ANIMATEDPARAMWRAPPER_H
#define SBK_ANIMATEDPARAMWRAPPER_H

#define protected public

#include <shiboken.h>

#include <ParameterWrapper.h>

class AnimatedParamWrapper : public AnimatedParam
{
public:
    virtual ~AnimatedParamWrapper();
    static void pysideInitQtMetaTypes();
};

#endif // SBK_ANIMATEDPARAMWRAPPER_H

