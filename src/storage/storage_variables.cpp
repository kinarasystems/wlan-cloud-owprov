//
// Created by stephane bourque on 2022-02-23.
//

#include "storage_variables.h"

#include "RESTObjects/RESTAPI_SecurityObjects.h"
#include "framework/OpenWifiTypes.h"
#include "framework/RESTAPI_utils.h"

namespace OpenWifi {

	const static ORM::FieldVec VariablesDB_Fields{
		// object info
		ORM::Field{"id", 64, true},
		ORM::Field{"name", ORM::FieldType::FT_TEXT},
		ORM::Field{"description", ORM::FieldType::FT_TEXT},
		ORM::Field{"notes", ORM::FieldType::FT_TEXT},
		ORM::Field{"created", ORM::FieldType::FT_BIGINT},
		ORM::Field{"modified", ORM::FieldType::FT_BIGINT},
		ORM::Field{"variables", ORM::FieldType::FT_TEXT},
		ORM::Field{"entity", ORM::FieldType::FT_TEXT},
		ORM::Field{"venue", ORM::FieldType::FT_TEXT},
		ORM::Field{"subscriber", ORM::FieldType::FT_TEXT},
		ORM::Field{"inventory", ORM::FieldType::FT_TEXT},
		ORM::Field{"configurations", ORM::FieldType::FT_TEXT},
		ORM::Field{"managementPolicy", ORM::FieldType::FT_TEXT},
		ORM::Field{"templateTag", ORM::FieldType::FT_TEXT}};

	const static ORM::IndexVec VariablesDB_Indexes{
		{std::string("variables_venue_index"),
		 ORM::IndexEntryVec{{std::string("venue"), ORM::Indextype::ASC}}},
		{std::string("variables_entity_index"),
		 ORM::IndexEntryVec{{std::string("entity"), ORM::Indextype::ASC}}}};

	VariablesDB::VariablesDB(OpenWifi::DBType T, Poco::Data::SessionPool &P,
							 Poco::Logger &L) noexcept
		: DB(T, "variables2", VariablesDB_Fields, VariablesDB_Indexes, P, L, "var") {}

	bool VariablesDB::Upgrade([[maybe_unused]] uint32_t from, uint32_t &to) {
		std::vector<std::string> Statements{
			            "alter table " + TableName_ + " add column templateTag TEXT;"
			//            "alter table " + TableName_ + " add column configurations TEXT;" ,
			//            "alter table " + TableName_ + " add column managementPolicy TEXT;"
		};
		if (!Statements.empty())
			RunScript(Statements);
		to = 2;
		return true;
	}
} // namespace OpenWifi

template <>
void ORM::DB<OpenWifi::VariablesDBRecordType, OpenWifi::ProvObjects::VariableBlock>::Convert(
	const OpenWifi::VariablesDBRecordType &In, OpenWifi::ProvObjects::VariableBlock &Out) {
	Out.info.id = In.get<0>();
	Out.info.name = In.get<1>();
	Out.info.description = In.get<2>();
	Out.info.notes =
		OpenWifi::RESTAPI_utils::to_object_array<OpenWifi::SecurityObjects::NoteInfo>(In.get<3>());
	Out.info.created = In.get<4>();
	Out.info.modified = In.get<5>();
	Out.variables =
		OpenWifi::RESTAPI_utils::to_object_array<OpenWifi::ProvObjects::Variable>(In.get<6>());
	Out.entity = In.get<7>();
	Out.venue = In.get<8>();
	Out.subscriber = In.get<9>();
	Out.inventory = In.get<10>();
	Out.configurations = OpenWifi::RESTAPI_utils::to_object_array(In.get<11>());
	Out.managementPolicy = In.get<12>();
	Out.templateTag = In.get<13>();
}

template <>
void ORM::DB<OpenWifi::VariablesDBRecordType, OpenWifi::ProvObjects::VariableBlock>::Convert(
	const OpenWifi::ProvObjects::VariableBlock &In, OpenWifi::VariablesDBRecordType &Out) {
	Out.set<0>(In.info.id);
	Out.set<1>(In.info.name);
	Out.set<2>(In.info.description);
	Out.set<3>(OpenWifi::RESTAPI_utils::to_string(In.info.notes));
	Out.set<4>(In.info.created);
	Out.set<5>(In.info.modified);
	Out.set<6>(OpenWifi::RESTAPI_utils::to_string(In.variables));
	Out.set<7>(In.entity);
	Out.set<8>(In.venue);
	Out.set<9>(In.subscriber);
	Out.set<10>(In.inventory);
	Out.set<11>(OpenWifi::RESTAPI_utils::to_string(In.configurations));
	Out.set<12>(In.managementPolicy);
	Out.set<13>(In.templateTag);
}
