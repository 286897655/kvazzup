#pragma once

#include <QtEndian>
#include <QByteArray>
#include <QHostAddress>

#include <vector>
#include <tuple>
#include <utility>
#include <memory>

const int TRANSACTION_ID_SIZE    = 12;
const uint32_t STUN_MAGIC_COOKIE = 0x2112A442;

enum STUN_TYPES
{
  STUN_REQUEST  = 0x0001,
  STUN_RESPONSE = 0x0101
};

enum STUN_ATTRIBUTES
{
  STUN_ATTR_XOR_MAPPED_ADDRESS = 0x0020,
  STUN_ATTR_PRIORITY           = 0x0024,
  STUN_ATTR_USE_CANDIATE       = 0x0025,
  STUN_ATTR_ICE_CONTROLLED     = 0x8029,
  STUN_ATTR_ICE_CONTROLLING    = 0x802A,
};

struct STUNRawMessage
{
  uint16_t type;
  uint16_t length;
  uint32_t magicCookie;
  uint8_t transactionID[TRANSACTION_ID_SIZE];
  unsigned char payload[0];
};

class STUNMessage
{
public:
  STUNMessage();
  STUNMessage(uint16_t type, uint16_t length);
  STUNMessage(uint16_t type);
  ~STUNMessage();

  void setType(uint16_t type);
  void setLength(uint16_t length);
  void setCookie(uint32_t cookie);

  void setTransactionID();
  void setTransactionID(uint8_t *transactionID);

  uint16_t getType();
  uint16_t getLength();
  uint32_t getCookie();

  // return pointer to message's transactionID array
  uint8_t *getTransactionID();
  uint8_t getTransactionIDAt(int index);

  // get all message's attributes
  /* std::vector<std::pair<uint16_t, uint16_t>>& getAttributes(); */
  std::vector<std::tuple<uint16_t, uint16_t, uint32_t>>& getAttributes();

  void addAttribute(uint16_t attribute);
  void addAttribute(uint16_t attribute, uint32_t value);

  // check if message has an attribute named "attrName" set
  // return true if yes and false if not
  bool hasAttribute(uint16_t attrName);

  // return true if the message contains xor-mapped-address and false if it doesn't
  // copy the address to info if possible
  bool getXorMappedAddress(std::pair<QHostAddress, uint16_t>& info);
  void setXorMappedAddress(QHostAddress address, uint16_t port);

private:
  uint16_t type_;
  uint16_t length_;
  uint32_t magicCookie_;
  uint8_t transactionID_[TRANSACTION_ID_SIZE];
  std::vector<std::tuple<uint16_t, uint16_t, uint32_t>> attributes_;
  std::pair<QHostAddress, uint16_t> mappedAddr_;
};

class StunMessageFactory
{
public:
  StunMessageFactory();
  ~StunMessageFactory();

  // Create STUN Binding Request message
  STUNMessage createRequest();

  // Create empty (no transactionID) STUN Binding response message
  STUNMessage createResponse();

  // Create STUN Binding response message
  STUNMessage createResponse(STUNMessage& request);

  // Convert from little endian to big endian and return the given STUN message as byte array
  QByteArray hostToNetwork(STUNMessage& message);

  // Conver from big endian to little endian and return the byte array as STUN message
  STUNMessage networkToHost(QByteArray& message);

  // return true if the saved transaction id and message's transaction id match
  bool verifyTransactionID(STUNMessage& message);

  bool validateStunRequest(STUNMessage& message);

  // validate stun response based on the latest request we have sent
  bool validateStunResponse(STUNMessage& response);

  // validate response based on 
  bool validateStunResponse(STUNMessage& response, QHostAddress sender, uint16_t port);

  // cache request to memory
  //
  // this request is used to verify the transactionID of received response
  void cacheRequest(STUNMessage request);

  // if we know we're going to receive response from some address:port combination
  // the transactionID can be saved to memory and then fetched for verification
  // when the response arrives
  //
  // This greatly improves the reliability of transactionID verification
  void expectReplyFrom(STUNMessage& request, QString address, uint16_t port);

private:
  // validate all fields of STUNMessage
  //
  // return true if message is valid, otherwise false
  bool validateStunMessage(STUNMessage& message, int type);

  // return the next attribute-value pair
  std::pair<uint16_t, uint16_t> getAttribute(uint16_t *ptr);

  // extract host address and port learned from STUN binding request to google server
  // this is just to make the code look nicer
  std::pair<QHostAddress, uint16_t> extractXorMappedAddress(uint16_t payloadLen, uint8_t *payload);

  // save transactionIDs by address and port
  QMap<QString, QMap<uint16_t, std::vector<uint8_t>>> expectedResponses_;

  STUNMessage latestRequest_;
};
