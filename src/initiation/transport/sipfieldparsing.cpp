#include "sipfieldparsing.h"

#include "sipconversions.h"

#include <QRegularExpression>
#include <QDebug>


// TODO: Support SIPS uri scheme. Needed for TLS
bool parseURI(QString values, SIP_URI& uri);
ConnectionType parseUritype(QString type);
bool parseParameterNameToValue(std::shared_ptr<QList<SIPParameter>> parameters,
                               QString name, QString& value);
bool parseUint(QString values, uint& number);


bool parseURI(QString values, SIP_URI& uri)
{
  // TODO: parse quotation marks in real name

  QRegularExpression re_field("(\\w+ )?<(\\w+):(\\w+)@([\\w.:]+)>");
  QRegularExpressionMatch field_match = re_field.match(values);

  // number of matches depends whether real name or the port were given
  if (field_match.hasMatch() &&
      field_match.lastCapturedIndex() >= 3 &&
      field_match.lastCapturedIndex() <= 4 )
  {
    QString addressString = "";

    if (field_match.lastCapturedIndex() == 4)
    {
      uri.realname = field_match.captured(1);
      uri.connectionType = parseUritype(field_match.captured(2));
      uri.username = field_match.captured(3);
      addressString = field_match.captured(4);
    }
    else if(field_match.lastCapturedIndex() == 3)
    {
      uri.connectionType = parseUritype(field_match.captured(1));
      uri.username = field_match.captured(2);
      addressString = field_match.captured(3);
    }

    QRegularExpression re_address("([\\w.]+):?(\\d*)");
    QRegularExpressionMatch address_match = re_address.match(addressString);

    if(address_match.hasMatch() &&
       address_match.lastCapturedIndex() >= 1 &&
      address_match.lastCapturedIndex() <= 2)
    {
      uri.host = address_match.captured(1);
      if(address_match.lastCapturedIndex() == 2)
      {
        uri.port = address_match.captured(2).toUInt();
      }
      else
      {
        uri.port = 0;
      }
    }
    else
    {
      return false;
    }

    return uri.connectionType != NONE;
  }

  return false;
}


ConnectionType parseUritype(QString type)
{
  if (type == "sip")
  {
    return TCP;
  }
  else if (type == "sips")
  {
    return TLS;
  }
  else if (type == "tel")
  {
    return TEL;
  }
  else
  {
    qDebug() << "ERROR:  Could not identify connection type:" << type;
  }

  return NONE;
}


bool parseParameterNameToValue(std::shared_ptr<QList<SIPParameter>> parameters,
                               QString name, QString& value)
{
  if(parameters != nullptr)
  {
    for(SIPParameter parameter : *parameters)
    {
      if(parameter.name == name)
      {
        value = parameter.value;
        return true;
      }
    }
  }
  return false;
}

bool parseUint(QString values, uint& number)
{
  QRegularExpression re_field("(\\d+)");
  QRegularExpressionMatch field_match = re_field.match(values);

  if(field_match.hasMatch() && field_match.lastCapturedIndex() == 1)
  {
    number = values.toUInt();
    return true;
  }
  return false;
}

bool parseToField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);
  Q_ASSERT(message->dialog);

  if(!parseURI(field.values, message->to))
  {
    return false;
  }

  parseParameterNameToValue(field.parameters, "tag", message->dialog->toTag);
  return true;
}

bool parseFromField(SIPField& field,
                    std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);
  Q_ASSERT(message->dialog);

  if(!parseURI(field.values, message->from))
  {
    return false;
  }
  parseParameterNameToValue(field.parameters, "tag", message->dialog->fromTag);
  return true;
}

bool parseCSeqField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);

  QRegularExpression re_field("(\\d+) (\\w+)");
  QRegularExpressionMatch field_match = re_field.match(field.values);

  if(field_match.hasMatch() && field_match.lastCapturedIndex() == 2)
  {
    message->cSeq = field_match.captured(1).toUInt();
    message->transactionRequest = stringToRequest(field_match.captured(2));
    return true;
  }
  return false;
}

bool parseCallIDField(SIPField& field,
                      std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);
  Q_ASSERT(message->dialog);

  QRegularExpression re_field("(\\S+)");
  QRegularExpressionMatch field_match = re_field.match(field.values);

  if(field_match.hasMatch() && field_match.lastCapturedIndex() == 1)
  {
    message->dialog->callID = field.values;
    return true;
  }

  return false;
}

bool parseViaField(SIPField& field,
                   std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);

  QRegularExpression re_field("SIP/(\\d.\\d)/(\\w+) ([\\w.]+):?(\\d*)");
  QRegularExpressionMatch field_match = re_field.match(field.values);

  if(!field_match.hasMatch() || field_match.lastCapturedIndex() < 3)
  {
    return false;
  }
  else if(field_match.lastCapturedIndex() == 3 ||
          field_match.lastCapturedIndex() == 4)
  {
    ViaInfo via = {stringToConnection(field_match.captured(2)),
                   field_match.captured(1),
                   field_match.captured(3), 0, ""}; // TODO: set port

    if (field_match.lastCapturedIndex() == 4)
    {
      via.port = field_match.captured(4).toUInt();
    }


    parseParameterNameToValue(field.parameters, "branch", via.branch);
    message->vias.push_back(via);
  }
  else
  {
    return false;
  }
  return true;
}

bool parseMaxForwardsField(SIPField& field,
                           std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);

  return parseUint(field.values, message->maxForwards);
}

bool parseContactField(SIPField& field,
                       std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);

  return parseURI(field.values, message->contact);
}

bool parseContentTypeField(SIPField& field,
                           std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);

  QRegularExpression re_field("(\\w+/\\w+)");
  QRegularExpressionMatch field_match = re_field.match(field.values);

  if(field_match.hasMatch() && field_match.lastCapturedIndex() == 1)
  {
    message->content.type = stringToContentType(field_match.captured(1));
    return true;
  }
  return false;
}

bool parseContentLengthField(SIPField& field,
                             std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);
  return parseUint(field.values, message->content.length);
}


bool parseServerField(SIPField& field,
                  std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message);
  message->server = field.values;

  return true;
}


bool isLinePresent(QString name, QList<SIPField>& fields)
{
  for(SIPField field : fields)
  {
    if(field.name == name)
    {
      return true;
    }
  }
  qDebug() << "Did not find header:" << name;
  return false;
}

bool parseParameter(QString text, SIPParameter& parameter)
{
  QRegularExpression re_parameter("([^=]+)=([^;]+)");
  QRegularExpressionMatch parameter_match = re_parameter.match(text);
  if(parameter_match.hasMatch() && parameter_match.lastCapturedIndex() == 2)
  {
    parameter.name = parameter_match.captured(1);
    parameter.value = parameter_match.captured(2);
    return true;
  }

  return false;
}
