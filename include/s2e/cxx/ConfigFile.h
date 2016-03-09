/*
 * S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2010, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Currently maintained by:
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *
 * All contributors are listed in the S2E-AUTHORS file.
 */

#ifndef S2E_CXX_CONFIG_FILE_H
#define S2E_CXX_CONFIG_FILE_H

#if !defined(__cplusplus)
#error This file is not supposed to be included from C!
#endif /* !defined(__cplusplus) */

#include <vector>
#include <string>
#include <inttypes.h>

#include "s2e/Lunar/Lunar.h"

namespace s2e {
class S2EExecutionState;

#define LUAS2E "LuaS2E"

class S2ELUAExecutionState
{
private:
    S2EExecutionState *m_state;
    uint64_t readParameterCdecl(lua_State *L, uint32_t param);
    bool writeParameterCdecl(lua_State *L, uint32_t param, uint64_t val);
    uint64_t readParameterAAPCS(lua_State *L, uint32_t param);
    bool writeParameterAAPCS(lua_State *L, uint32_t param, uint64_t val);

public:
  static const char className[];
  static Lunar<S2ELUAExecutionState>::RegType methods[];

  S2ELUAExecutionState(lua_State *L);
  S2ELUAExecutionState(S2EExecutionState *s);
  ~S2ELUAExecutionState();
  int writeRegister(lua_State *L);
  int writeRegisterSymb(lua_State *L);
  int readRegister(lua_State *L);
  int readParameter(lua_State *L);
  int writeParameter(lua_State *L);
  int writeMemorySymb(lua_State *L);
  int readMemory(lua_State *L);
  int writeMemory(lua_State *L);
  int isSpeculative(lua_State *L);
  int getID(lua_State *L);
};


class ConfigFile
{
private:
    lua_State *m_luaState;

    /* Called on errors during initial loading. Will terminate the program */
    void luaError(const char *fmt, ...);

    /* Called on errors that can be ignored */
    void luaWarning(const char *fmt, ...);

    /* Fake data type for list size */
    struct _list_size { int size; };

    /* Fake data type for table key list */
    struct _key_list { std::vector<std::string> keys; };

    /* Helper to get C++ type name */
    template<typename T>
    const char* getTypeName();

    /* Helper to get topmost value of lua stack as a C++ value */
    template<typename T>
    bool getLuaValue(T* res, const T& def, int index = -1);

    /* Universal implementation for getXXX functions */
    template<typename T>
    T getValueT(const std::string& expr, const T& def, bool *ok);

    int RegisterS2EApi();

public:
    ConfigFile(const std::string &configFileName);
    ~ConfigFile();

    /* Return value from configuration file.
  
       Example:
         width = getValueInt("window.width");
  
       Arguments:
         name  the name or the value (actually,
               any valid lua expression that will be
               prepended by "return ")
         def   default value to return on error
         ok    if non-null then will be false on error
    */
    bool getBool(const std::string& name, bool def = false, bool *ok = NULL);
    int64_t getInt(const std::string& name, int64_t def = 0, bool *ok = NULL);
    double getDouble(const std::string& name, double def = 0, bool *ok = NULL);
    std::string getString(const std::string& name,
                    const std::string& def = std::string(), bool *ok = NULL);
  
    typedef std::vector<std::string> string_list;
    string_list getStringList(const std::string& name,
                    const string_list& def = string_list(), bool *ok = NULL);

    typedef std::vector<uint64_t> integer_list;
    integer_list getIntegerList(
            const std::string& name, const integer_list& def = integer_list(), bool *ok = NULL);


    /* Return all string keys for a given table.
       Fails if some keys are not strings. */
    string_list getListKeys(const std::string& name, bool *ok = NULL);
    
    /* Return the size of the list. Works for all types of
       lua lists just like '#' operator in lua. */
    int getListSize(const std::string& name, bool *ok = NULL);

    /* Returns true if a config key exists */
    bool hasKey(const std::string& name);


    void invokeLuaCommand(const char *cmd);

    //void invokeAnnotation(const std::string &annotation, S2EExecutionState *param);

    lua_State* getState() const {
        return m_luaState;
    }

    bool isFunctionDefined(const std::string &name) const;

};

} // namespace s2e

#endif /* S2E_CXX_CONFIG_FILE_H */
