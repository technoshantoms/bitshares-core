/*
 * AcloudBank
 */
#include <graphene/protocol/memo.hpp>
#include <boost/endian/conversion.hpp>
#include <fc/crypto/aes.hpp>
#include <fc/io/raw.hpp>

namespace graphene { namespace protocol {

void memo_data::set_message(const fc::ecc::private_key& priv, const fc::ecc::public_key& pub,
                            const string& msg, uint64_t custom_nonce)
{
   if( priv != fc::ecc::private_key() && public_key_type(pub) != public_key_type() )
   {
      from = priv.get_public_key();
      to = pub;
      if( 0 == custom_nonce )
      {
         uint64_t entropy = fc::sha224::hash(fc::ecc::private_key::generate())._hash[0].value();
         constexpr uint64_t half_size = 32;
         constexpr uint64_t high_bits = 0xff00000000000000ULL;
         constexpr uint64_t  low_bits = 0x00ffffffffffffffULL;
         entropy <<= half_size;
         entropy &= high_bits;
         nonce = ((uint64_t)(fc::time_point::now().time_since_epoch().count()) & low_bits) | entropy;
      } else
         nonce = custom_nonce;
      auto secret = priv.get_shared_secret(pub);
      auto nonce_plus_secret = fc::sha512::hash(fc::to_string(nonce) + secret.str());
      string text = memo_message((uint32_t)digest_type::hash(msg)._hash[0].value(), msg).serialize();
      message = fc::aes_encrypt( nonce_plus_secret, vector<char>(text.begin(), text.end()) );
   }
   else
   {
      auto text = memo_message(0, msg).serialize();
      message = vector<char>(text.begin(), text.end());
   }
}

string memo_data::get_message(const fc::ecc::private_key& priv,
                              const fc::ecc::public_key& pub)const
{
   if( from != public_key_type() )
   {
      auto secret = priv.get_shared_secret(pub);
      auto nonce_plus_secret = fc::sha512::hash(fc::to_string(nonce) + secret.str());
      auto plain_text = fc::aes_decrypt( nonce_plus_secret, message );
      auto result = memo_message::deserialize(string(plain_text.begin(), plain_text.end()));
      FC_ASSERT( result.checksum == (uint32_t)digest_type::hash(result.text)._hash[0].value() );
      return result.text;
   }
   else
   {
      return memo_message::deserialize(string(message.begin(), message.end())).text;
   }
}

string memo_message::serialize() const
{
   auto serial_checksum = string(sizeof(checksum), ' ');
   (uint32_t&)(*serial_checksum.data()) = boost::endian::native_to_little(checksum);
   return serial_checksum + text;
}

memo_message memo_message::deserialize(const string& serial)
{
   memo_message result;
   FC_ASSERT( serial.size() >= sizeof(result.checksum) );
   result.checksum = boost::endian::little_to_native((uint32_t&)(*serial.data()));
   result.text = serial.substr(sizeof(result.checksum));
   return result;
}

} } // graphene::protocol

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::memo_message )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::memo_data )
