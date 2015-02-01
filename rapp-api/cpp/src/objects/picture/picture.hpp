#ifndef RAPP_OBJECT_PICTURE
#define RAPP_OBJECT_PICTURE
#include "Includes.ihh"

namespace rapp {
namespace object {

/**
 * @class picture
 * @brief class which wraps around raw bytes of a picture
 * @version 1
 * @date 25-January-2015
 * @author Alex Gkiokas <a.gkiokas@ortelio.co.uk>
 * 
 */

class picture
{
  public:

    typedef char byte;
    
    picture ( ) = delete;

    /// Construct from a file-path
    picture ( const std::string filepath );

    /// Construct using an open file stream
    picture ( std::ifstream & bytestream );

    /// Construct using raw byte array
    picture ( std::vector<byte> bytearray );

    /// Copy constructor
    picture ( const picture & ) = default;

    /// Destructor
    ~picture ( );

    /// Get picture as array of bytes
    std::vector<byte> bytearray ( ) const;

    /// Are pictures same ?
    bool operator== ( const picture & ) const;

    /// Assignment operator
    picture & operator= ( const picture & ) = default;

    /// Save picture to filepath
    bool save ( const std::string filepath );

    /// Print buffer on std::out
    void echo ( ) const;

  private:

    // Parse the bytestream into the bytearray
    void openCopy_ ( );
    

    std::ifstream bytestream_;
    
    std::vector<byte> bytearray_;
};

}
}
#endif