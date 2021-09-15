//
// Created by stephane bourque on 2021-08-23.
//

#ifndef OWPROV_RESTAPI_CONTACT_LIST_HANDLER_H
#define OWPROV_RESTAPI_CONTACT_LIST_HANDLER_H

#include "RESTAPI_handler.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "RESTAPI_ProvObjects.h"

namespace OpenWifi {

    class RESTAPI_contact_list_handler : public RESTAPIHandler {
    public:
        RESTAPI_contact_list_handler(const RESTAPIHandler::BindingMap &bindings, Poco::Logger &L, RESTAPI_GenericServer & Server, bool Internal)
        : RESTAPIHandler(bindings, L,
                         std::vector<std::string>{
            Poco::Net::HTTPRequest::HTTP_GET,
            Poco::Net::HTTPRequest::HTTP_OPTIONS},
            Server,
            Internal) {}
        static const std::list<const char *> PathName() { return std::list<const char *>{"/api/v1/contact"}; };

        void DoGet() final;
        void DoPost() final {};
        void DoPut() final {};
        void DoDelete() final {};
    };
}


#endif //OWPROV_RESTAPI_CONTACT_LIST_HANDLER_H
