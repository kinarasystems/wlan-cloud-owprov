//
//	License type: BSD 3-Clause License
//	License copy: https://github.com/Telecominfraproject/wlan-cloud-ucentralgw/blob/master/LICENSE
//
//	Created by Stephane Bourque on 2021-03-04.
//	Arilia Wireless Inc.
//

#include "framework/MicroService.h"

#include "RESTAPI_configurations_handler.h"
#include "RESTObjects/RESTAPI_ProvObjects.h"
#include "StorageService.h"
#include "framework/ConfigurationValidator.h"
#include "RESTAPI/RESTAPI_db_helpers.h"
#include "DeviceTypeCache.h"

namespace OpenWifi{

    void RESTAPI_configurations_handler::DoGet() {
        std::string UUID = GetBinding("uuid","");
        ProvObjects::DeviceConfiguration   Existing;
        if(UUID.empty() || !DB_.GetRecord("id", UUID, Existing)) {
            return NotFound();
        }

        Poco::JSON::Object  Answer;
        std::string Arg;
        if(HasParameter("expandInUse",Arg) && Arg=="true") {
            Storage::ExpandedListMap    M;
            std::vector<std::string>    Errors;
            Poco::JSON::Object          Inner;
            if(StorageService()->ExpandInUse(Existing.inUse,M,Errors)) {
                for(const auto &[type,list]:M) {
                    Poco::JSON::Array   ObjList;
                    for(const auto &i:list.entries) {
                        Poco::JSON::Object  O;
                        i.to_json(O);
                        ObjList.add(O);
                    }
                    Inner.set(type,ObjList);
                }
            }
            Answer.set("entries", Inner);
            return ReturnObject(Answer);
        } else if(HasParameter("computedAffected",Arg) && Arg=="true") {
            Types::UUIDvec_t DeviceSerialNumbers;
            DB_.GetListOfAffectedDevices(UUID,DeviceSerialNumbers);
            return ReturnObject("affectedDevices", DeviceSerialNumbers);
        } else if(QB_.AdditionalInfo) {
            AddExtendedInfo(Existing,Answer);
        }
        Existing.to_json(Answer);
        ReturnObject(Answer);
    }

    void RESTAPI_configurations_handler::DoDelete() {
        std::string UUID = GetBinding("uuid","");
        ProvObjects::DeviceConfiguration   Existing;
        if(UUID.empty() || !DB_.GetRecord("id", UUID, Existing)) {
            return NotFound();
        }

        if(!Existing.inUse.empty()) {
            return BadRequest(RESTAPI::Errors::StillInUse);
        }

        DB_.DeleteRecord("id", UUID);
        MoveUsage(StorageService()->PolicyDB(),DB_,Existing.managementPolicy,"",Existing.info.id);
        for(const auto &i:Existing.variables)
            RemoveMembership(StorageService()->VariablesDB(),&ProvObjects::VariableBlock::configurations,i,Existing.info.id);

        return OK();
    }

    bool RESTAPI_configurations_handler::ValidateConfigBlock(const ProvObjects::DeviceConfiguration &Config, std::string & Error) {
        static const std::vector<std::string> SectionNames{ "globals", "interfaces", "metrics", "radios", "services", "unit" };

        for(const auto &i:Config.configuration) {
            Poco::JSON::Parser  P;
            if(i.name.empty()) {
                std::cout << "Name is empty" << std::endl;
                BadRequest(RESTAPI::Errors::NameMustBeSet);
                return false;
            }

            try {
                auto Blocks = P.parse(i.configuration).extract<Poco::JSON::Object::Ptr>();
                auto N = Blocks->getNames();
                for (const auto &j: N) {
                    if (std::find(SectionNames.cbegin(), SectionNames.cend(), j) == SectionNames.cend()) {
                        std::cout << "Block section name rejected: " << j << std::endl;
                        BadRequest(RESTAPI::Errors::ConfigBlockInvalid);
                        return false;
                    }
                }
            } catch (const Poco::JSON::JSONException &E ) {
                Error = "Block " + i.name + " failed parsing: " + E.message();
                return false;
            }

            try {
                if (ValidateUCentralConfiguration(i.configuration, Error)) {
                    std::cout << "Block: " << i.name << " is valid" << std::endl;
                } else {
                    std::cout << "Block: " << std::endl << ">>>" << std::endl << i.configuration << std::endl
                              << ">>> REJECTED" << std::endl;
                    return false;
                }
            } catch(...) {
                std::cout << "Exception in validation" << std::endl;
                return false;
            }

        }
        return true;
    }

    void RESTAPI_configurations_handler::DoPost() {
        auto UUID = GetBinding("uuid","");
        if(UUID.empty()) {
            return BadRequest(RESTAPI::Errors::MissingUUID);
        }

        std::string Arg;
        if(HasParameter("validateOnly",Arg) && Arg=="true") {
            auto Body = ParseStream();
            if(!Body->has("configuration")) {
                return BadRequest("Must have 'configuration' element.");
            }
            auto Config=Body->get("configuration").toString();
            Poco::JSON::Object  Answer;
            std::string Error;
            auto Res = ValidateUCentralConfiguration(Config,Error);
            Answer.set("valid",Res);
            Answer.set("error", Error);
            return ReturnObject(Answer);
        }

        ProvObjects::DeviceConfiguration C;
        Poco::JSON::Object::Ptr Obj = ParseStream();
        if (!C.from_json(Obj)) {
            return BadRequest(RESTAPI::Errors::InvalidJSONDocument);
        }

        if(!ProvObjects::CreateObjectInfo(Obj,UserInfo_.userinfo,C.info)) {
            return BadRequest(RESTAPI::Errors::NameMustBeSet);
        }

        if(!C.managementPolicy.empty() && !StorageService()->PolicyDB().Exists("id",C.managementPolicy)) {
            return BadRequest(RESTAPI::Errors::UnknownId);
        }

        C.inUse.clear();
        if(C.deviceTypes.empty() || !DeviceTypeCache()->AreAcceptableDeviceTypes(C.deviceTypes, true)) {
            return BadRequest(RESTAPI::Errors::InvalidDeviceTypes);
        }

        std::string Error;
        if(!ValidateConfigBlock(C,Error)) {
            return BadRequest(RESTAPI::Errors::ConfigBlockInvalid + ", error: " + Error);
        }

        if(DB_.CreateRecord(C)) {
            MoveUsage(StorageService()->PolicyDB(),DB_,"",C.managementPolicy,C.info.id);
            DB_.GetRecord("id", C.info.id, C);
            Poco::JSON::Object  Answer;
            C.to_json(Answer);
            return ReturnObject(Answer);
        }
        InternalError(RESTAPI::Errors::RecordNotCreated);
    }

    void RESTAPI_configurations_handler::DoPut() {
        auto UUID = GetBinding("uuid","");
        ProvObjects::DeviceConfiguration    Existing;
        if(UUID.empty() || !DB_.GetRecord("id", UUID, Existing)) {
            return NotFound();
        }

        ProvObjects::DeviceConfiguration    NewConfig;
        auto ParsedObj = ParseStream();
        if (!NewConfig.from_json(ParsedObj)) {
            return BadRequest(RESTAPI::Errors::InvalidJSONDocument);
        }

        if(!UpdateObjectInfo(ParsedObj, UserInfo_.userinfo, Existing.info)) {
            return BadRequest(RESTAPI::Errors::NameMustBeSet);
        }

        if(!NewConfig.deviceTypes.empty() && !DeviceTypeCache()->AreAcceptableDeviceTypes(NewConfig.deviceTypes, true)) {
            return BadRequest(RESTAPI::Errors::InvalidDeviceTypes);
        }

        if(!NewConfig.deviceTypes.empty())
            Existing.deviceTypes = NewConfig.deviceTypes;

        std::string Error;
        if(!ValidateConfigBlock( NewConfig,Error)) {
            return BadRequest(RESTAPI::Errors::ConfigBlockInvalid + ", error: " + Error);
        }

        if(ParsedObj->has("configuration")) {
            Existing.configuration = NewConfig.configuration;
        }

        std::string MovePolicy, ExistingPolicy;
        if(AssignIfPresent(ParsedObj,"managementPolicy",MovePolicy)) {
            if(!MovePolicy.empty() && !StorageService()->PolicyDB().Exists("id",NewConfig.managementPolicy)) {
                return BadRequest(RESTAPI::Errors::UnknownManagementPolicyUUID);
            }
            ExistingPolicy = Existing.managementPolicy;
        }

        Types::UUIDvec_t FromVariables, ToVariables;
        if(ParsedObj->has("variables")) {
            for(const auto &i:NewConfig.variables) {
                if(!i.empty() && !StorageService()->VariablesDB().Exists("id",i)) {
                    return BadRequest(RESTAPI::Errors::VariableMustExist);
                }
            }
            for(const auto &i:Existing.variables)
                FromVariables.emplace_back(i);
            for(const auto &i:NewConfig.variables)
                ToVariables.emplace_back(i);
            FromVariables = Existing.variables;
            ToVariables = NewConfig.variables;
            Existing.variables = ToVariables;
        }

        if(AssignIfPresent(ParsedObj,"managementPolicy",MovePolicy)) {
            if(!MovePolicy.empty() && !StorageService()->PolicyDB().Exists("id",NewConfig.managementPolicy)) {
                return BadRequest(RESTAPI::Errors::UnknownManagementPolicyUUID);
            }
            ExistingPolicy = Existing.managementPolicy;
        }

        AssignIfPresent(ParsedObj,  "rrm", Existing.rrm);
        AssignIfPresent(ParsedObj,  "firmwareUpgrade",Existing.firmwareUpgrade);
        AssignIfPresent(ParsedObj,  "firmwareRCOnly", Existing.firmwareRCOnly);

        if(DB_.UpdateRecord("id",UUID,Existing)) {
            MoveUsage(StorageService()->PolicyDB(), DB_, ExistingPolicy, MovePolicy, Existing.info.id);
            ManageMembership(StorageService()->VariablesDB(),&ProvObjects::VariableBlock::configurations, FromVariables, ToVariables, Existing.info.id);

            ProvObjects::DeviceConfiguration    D;
            DB_.GetRecord("id",UUID,D);
            Poco::JSON::Object  Answer;
            D.to_json(Answer);
            return ReturnObject(Answer);
        }
        InternalError(RESTAPI::Errors::RecordNotUpdated);
    }
}