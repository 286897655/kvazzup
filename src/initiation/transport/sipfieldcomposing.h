#pragma once

#include "initiation/siptypes.h"


bool getFirstRequestLine(QString& line, SIPRequest& request, QString lineEnding);
bool getFirstResponseLine(QString& line, SIPResponse& response, QString lineEnding);

// RFC3261_TODO: Accept header would be nice

// These functions work as follows: Create a field based on necessary info
// from the message and add the field to list. Later the fields are converted to string.

bool includeToField(QList<SIPField>& fields,
                    std::shared_ptr<SIPMessageInfo> message);

bool includeFromField(QList<SIPField>& fields,
                      std::shared_ptr<SIPMessageInfo> message);

bool includeCSeqField(QList<SIPField>& fields,
                      std::shared_ptr<SIPMessageInfo> message);

bool includeCallIDField(QList<SIPField>& fields,
                        std::shared_ptr<SIPMessageInfo> message);

bool includeViaFields(QList<SIPField>& fields,
                      std::shared_ptr<SIPMessageInfo> message);

bool includeMaxForwardsField(QList<SIPField>& fields,
                             std::shared_ptr<SIPMessageInfo> message);

bool includeContactField(QList<SIPField>& fields,
                         std::shared_ptr<SIPMessageInfo> message);

bool includeContentTypeField(QList<SIPField>& fields,
                             QString contentType);

bool includeContentLengthField(QList<SIPField>& fields,
                               uint32_t contentLenght);
