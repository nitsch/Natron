#ifndef SBK_BOOLEANPARAMWRAPPER_H
#define SBK_BOOLEANPARAMWRAPPER_H

#define protected public

#include <shiboken.h>

#include <ParameterWrapper.h>

class BooleanParamWrapper : public BooleanParam
{
public:
    virtual ~BooleanParamWrapper();
    static void pysideInitQtMetaTypes();
};

#endif // SBK_BOOLEANPARAMWRAPPER_H

