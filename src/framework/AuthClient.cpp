//
// Created by stephane bourque on 2022-10-25.
//

#include "Poco/Net/HTTPServerResponse.h"

#include "fmt/format.h"
#include "framework/AuthClient.h"
#include "framework/MicroServiceNames.h"
#include "framework/OpenAPIRequests.h"
#include "framework/utils.h"

namespace OpenWifi {

	bool AuthClient::RetrieveTokenInformation(const std::string &SessionToken,
											  SecurityObjects::UserInfoAndPolicy &UInfo,
											  std::uint64_t TID, bool &Expired, bool &Contacted,
											  bool Sub) {
		try {
			Types::StringPairVec QueryData;
			QueryData.push_back(std::make_pair("token", SessionToken));
			std::string AlternateURIForLogging = fmt::format(
				"{}?token={}", Sub ? "/api/v1/validateSubToken" : "/api/v1/validateToken",
				Utils::SanitizeToken(SessionToken));
			OpenAPIRequestGet Req(uSERVICE_SECURITY,
								  Sub ? "/api/v1/validateSubToken" : "/api/v1/validateToken",
								  QueryData, 10000, AlternateURIForLogging);
			Poco::JSON::Object::Ptr Response;

			auto StatusCode = Req.Do(Response);
			if (StatusCode == Poco::Net::HTTPServerResponse::HTTP_GATEWAY_TIMEOUT) {
				Contacted = false;
				return false;
			}

			Contacted = true;
			if (StatusCode == Poco::Net::HTTPServerResponse::HTTP_OK) {
				if (Response->has("tokenInfo") && Response->has("userInfo")) {
					UInfo.from_json(Response);
					if (IsTokenExpired(UInfo.webtoken)) {
						Expired = true;
						return false;
					}
					Expired = false;
					Cache_.update(SessionToken, UInfo);
					return true;
				} else {
					return false;
				}
			}
		} catch (...) {
			poco_error(Logger(), fmt::format("Failed to retrieve token={} for TID={}",
											 Utils::SanitizeToken(SessionToken), TID));
		}
		Expired = false;
		return false;
	}

	bool AuthClient::IsAuthorized(const std::string &SessionToken,
								  SecurityObjects::UserInfoAndPolicy &UInfo, std::uint64_t TID,
								  bool &Expired, bool &Contacted, bool Sub) {
		auto User = Cache_.get(SessionToken);
		if (!User.isNull()) {
			if (IsTokenExpired(User->webtoken)) {
				Expired = true;
				Cache_.remove(SessionToken);
				return false;
			}
			Expired = false;
			UInfo = *User;
			return true;
		}
		return RetrieveTokenInformation(SessionToken, UInfo, TID, Expired, Contacted, Sub);
	}

	bool AuthClient::RetrieveApiKeyInformation(const std::string &SessionToken,
											   SecurityObjects::UserInfoAndPolicy &UInfo,
											   std::uint64_t TID, bool &Expired, bool &Contacted,
											   [[maybe_unused]] bool &Suspended) {
		try {
			Types::StringPairVec QueryData;
			QueryData.push_back(std::make_pair("apikey", SessionToken));
			std::string AlternateURIForLogging =
				fmt::format("/api/v1/validateApiKey?apiKey={}", Utils::SanitizeToken(SessionToken));
			OpenAPIRequestGet Req(uSERVICE_SECURITY, "/api/v1/validateApiKey", QueryData, 10000,
								  AlternateURIForLogging);
			Poco::JSON::Object::Ptr Response;

			auto StatusCode = Req.Do(Response);
			if (StatusCode == Poco::Net::HTTPServerResponse::HTTP_GATEWAY_TIMEOUT) {
				Contacted = false;
				return false;
			}

			Contacted = true;
			if (StatusCode == Poco::Net::HTTPServerResponse::HTTP_OK) {
				if (Response->has("tokenInfo") && Response->has("userInfo") &&
					Response->has("expiresOn")) {
					UInfo.from_json(Response);
					Expired = false;
					ApiKeyCache_.update(SessionToken,
										ApiKeyCacheEntry{.UserInfo = UInfo,
														 .ExpiresOn = Response->get("expiresOn")});
					return true;
				} else {
					return false;
				}
			}
		} catch (...) {
			poco_error(Logger(), fmt::format("Failed to retrieve api key={} for TID={}",
											 Utils::SanitizeToken(SessionToken), TID));
		}
		Expired = false;
		return false;
	}

	bool AuthClient::IsValidApiKey(const std::string &SessionToken,
								   SecurityObjects::UserInfoAndPolicy &UInfo, std::uint64_t TID,
								   bool &Expired, bool &Contacted, bool &Suspended) {
		auto User = ApiKeyCache_.get(SessionToken);
		if (!User.isNull()) {
			if (User->ExpiresOn < Utils::Now()) {
				Expired = false;
				UInfo = User->UserInfo;
				return true;
			}
			ApiKeyCache_.remove(SessionToken);
		}
		return RetrieveApiKeyInformation(SessionToken, UInfo, TID, Expired, Contacted, Suspended);
	}

	/**
	 * Given a role, remove the cached user info for any user with that role
	*/
	void AuthClient::EmptyCacheForRole(const std::string &role) {
		SecurityObjects::USER_ROLE roleEnum = SecurityObjects::UserTypeFromString(role);
		Poco::JSON::Object::ConstIterator it;
		std::set<std::string> tokens = Cache_.getAllKeys();
		for(const std::string &token : tokens) {
			auto UInfo = Cache_.get(token);
			if (UInfo->userinfo.userRole == roleEnum) {
				Cache_.remove(token);
			}
		}

		tokens = ApiKeyCache_.getAllKeys();
		for(const std::string &token : tokens) {
			auto UInfo = ApiKeyCache_.get(token);
			if (UInfo->UserInfo.userinfo.userRole == roleEnum) {
				ApiKeyCache_.remove(token);
			}
		}
	}
} // namespace OpenWifi