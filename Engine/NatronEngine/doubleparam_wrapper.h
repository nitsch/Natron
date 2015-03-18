#ifndef SBK_DOUBLEPARAMWRAPPER_H
#define SBK_DOUBLEPARAMWRAPPER_H

#define protected public

#include <shiboken.h>

#include <ParameterWrapper.h>

class DoubleParamWrapper : public DoubleParam
{
public:
    virtual ~DoubleParamWrapper();
    static void pysideInitQtMetaTypes();
};

#endif // SBK_DOUBLEPARAMWRAPPER_H

