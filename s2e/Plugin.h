#ifndef S2E_PLUGIN_H
#define S2E_PLUGIN_H

#include <string>
#include <vector>
#include <tr1/unordered_map>

#include <sigc++/sigc++.h>

namespace s2e {

class S2E;
struct PluginInfo;
class PluginState;
class S2EExecutionState;

class Plugin : public sigc::trackable{
private:
    S2E* m_s2e;
protected:
    PluginState *m_CachedPluginState;
    S2EExecutionState *m_CachedPluginS2EState;

public:
    Plugin(S2E* s2e) : m_s2e(s2e),m_CachedPluginState(NULL),
        m_CachedPluginS2EState(NULL) {}

    virtual ~Plugin() {}

    /** Return assosiated S2E instance */
    S2E* s2e() { return m_s2e; }

    /** Initialize plugin. This function is called on initialization
        after all plugin instances have already be instantied */
    virtual void initialize();

    /** Return PluginInfo for this class. Defined by S2E_PLUGIN macro */
    virtual const PluginInfo* getPluginInfo() const = 0;

    /** Return configuration key for this plugin */
    const std::string& getConfigKey() const;

    PluginState *getPluginState(S2EExecutionState *s, PluginState* (*f)());

};

#define DECLARE_PLUGINSTATE(c, execstate) \
    c *plgState = (c*)getPluginState(execstate, &c::factory)

class PluginState
{
public:
    virtual ~PluginState() {};
    virtual PluginState *clone() const = 0;
};


struct PluginInfo {
    /** Unique name of the plugin */
    std::string name;

    /** Human-readable description of the plugin */
    std::string description;

    /** Name of a plugin function (only one plugin is allowed for each function) */
    std::string functionName;

    /** Dependencies of this plugin */
    std::vector<std::string> dependencies;

    /** Configuration key for this plugin */
    std::string configKey;

    /** A function to create a plugin instance */
    Plugin* (*instanceCreator)(S2E*);
};

class PluginsFactory {
private:
    std::tr1::unordered_map<std::string, const PluginInfo*> m_pluginsMap;
    std::vector<const PluginInfo*> m_pluginsList;

public:
    PluginsFactory();

    void registerPlugin(const PluginInfo* pluginInfo);

    const std::vector<const PluginInfo*> &getPluginInfoList() const;
    const PluginInfo* getPluginInfo(const std::string& name) const;

    Plugin* createPlugin(S2E* s2e, const std::string& name) const;
};

/** Should be put at the begining of any S2E plugin */
#define S2E_PLUGIN                                                                 \
    private:                                                                       \
        static const char s_pluginDeps[][64];                                      \
        static const PluginInfo s_pluginInfo;                                      \
    public:                                                                        \
        virtual const PluginInfo* getPluginInfo() const { return &s_pluginInfo; }  \
        static  const PluginInfo* getPluginInfoStatic() { return &s_pluginInfo; }  \
    private:

/** Defines an S2E plugin. Should be put in a cpp file.
    NOTE: use S2E_NOOP from Utils.h to pass multiple dependencies */
#define S2E_DEFINE_PLUGIN(className, description, functionName, ...)      \
    const char className::s_pluginDeps[][64] = { __VA_ARGS__ };                   \
    const PluginInfo className::s_pluginInfo = {                                   \
        #className, description, functionName,                                     \
        std::vector<std::string>(className::s_pluginDeps, className::s_pluginDeps  \
            + sizeof(className::s_pluginDeps)/sizeof(className::s_pluginDeps[0])), \
         "pluginsConfig['" #className "']",                                        \
        _pluginCreatorHelper<className>                                            \
    }

template<class C>
Plugin* _pluginCreatorHelper(S2E* s2e) { return new C(s2e); }

inline const std::string& Plugin::getConfigKey() const {
    return getPluginInfo()->configKey;
}

} // namespace s2e

#endif // S2E_PLUGIN_H
