#ifndef CLOUD_TYPES_H
#define CLOUD_TYPES_H

#include <cctype>
#include <string>
#include <vector>
#include <algorithm>

namespace Cloud
{

    enum class PropertyType
    {
        BOOL,
        INT,
        FLOAT,
        STRING,
        UNKNOWN
    };

    enum class Permission
    {
        READ,
        READWRITE
    };

    enum class OnChangeAction
    {
        NONE,
        ON_CHANGE
    };

    struct CloudProperty
    {
        std::string name;
        PropertyType type = PropertyType::UNKNOWN;
        Permission permission = Permission::READWRITE;
        OnChangeAction onChange = OnChangeAction::NONE;
        float initialValue = 0.0f;
    };

    struct CloudBinding
    {
        std::string cloudProperty;
        int ioPin = 0;
        std::string ioType;

        float cloudMin = 0.0f;
        float cloudMax = 100.0f;
        float ioMin = 0.0f;
        float ioMax = 10.0f;
        bool invertLogic = false;
    };

    struct CloudConfig
    {
        bool enabled = false;

        std::string thingId;
        std::string deviceName;

        // Credentials
        std::string deviceId;
        std::string deviceSecret;

        std::vector<CloudProperty> properties;
        std::vector<CloudBinding> bindings;
    };

    inline std::string toUpper(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
                       { return (char)std::toupper(c); });
        return s;
    }

    inline PropertyType stringToPropertyType(const std::string &type)
    {
        const auto t = toUpper(type);
        if (t == "BOOL" || t == "BOOLEAN")
            return PropertyType::BOOL;
        if (t == "INT" || t == "INTEGER")
            return PropertyType::INT;
        if (t == "FLOAT" || t == "DOUBLE" || t == "NUMBER")
            return PropertyType::FLOAT;
        if (t == "STRING")
            return PropertyType::STRING;
        return PropertyType::UNKNOWN;
    }

    inline Permission stringToPermission(const std::string &perm)
    {
        const auto p = toUpper(perm);
        if (p == "READ")
            return Permission::READ;
        return Permission::READWRITE;
    }

    inline OnChangeAction stringToOnChangeAction(const std::string &action)
    {
        const auto a = toUpper(action);
        if (a == "ON_CHANGE")
            return OnChangeAction::ON_CHANGE;
        return OnChangeAction::NONE;
    }

} // namespace Cloud

#endif // CLOUD_TYPES_H
