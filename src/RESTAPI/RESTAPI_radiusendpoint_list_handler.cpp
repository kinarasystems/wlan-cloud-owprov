//
// Created by stephane bourque on 2023-09-27.
//

#include "RESTAPI_radiusendpoint_list_handler.h"

namespace OpenWifi {

    void RESTAPI_radiusendpoint_list_handler::DoGet() {

        if(QB_.CountOnly) {
            return ReturnCountOnly(DB_.Count());
        }

        std::vector<ProvObjects::RADIUSEndPoint>    Records;
        if(DB_.GetRecords(QB_.Offset,QB_.Limit,Records)) {
            return ReturnObject(Records);
        }
        return NotFound();
    }

} // OpenWifi