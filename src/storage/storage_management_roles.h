//
//	License type: BSD 3-Clause License
//	License copy: https://github.com/Telecominfraproject/wlan-cloud-ucentralgw/blob/master/LICENSE
//
//	Created by Stephane Bourque on 2021-03-04.
//	Arilia Wireless Inc.
//

#pragma once

#include "framework/orm.h"
#include "RESTObjects/RESTAPI_ProvObjects.h"

namespace OpenWifi {
    typedef Poco::Tuple<
        std::string,
        std::string,
        std::string,
        std::string,
        uint64_t,
        uint64_t,
        std::string,
        std::string,
        std::string,
        std::string,
        std::string
    > ManagementRoleDBRecordType;

    class ManagementRoleDB : public ORM::DB<ManagementRoleDBRecordType, ProvObjects::ManagementRole> {
        public:
            ManagementRoleDB( OpenWifi::DBType T, Poco::Data::SessionPool & P, Poco::Logger &L);
        private:
    };
}
