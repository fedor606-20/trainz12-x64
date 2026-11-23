// U16: local owner of the texture data that used to come from the original module: the two exported
// SharedTextureCollection arrays and the format-name table TextureManager::Init reads.
#pragma once

// ?textureArray@SharedTextureCollection@Jet@@1PAV?$AutoReference@VTexture@Jet@@@2@A
// Eleven AutoReference slots (a Texture* and its one reference), 44 bytes.
extern unsigned char RsRsD068[44];
// ?textureNameArray@SharedTextureCollection@Jet@@1PAVPString@2@A: eleven PStrings, 44 bytes.
extern unsigned char RsRsD069[44];

namespace Jet {

class PString;
class Texture;

namespace TextureData {

// The 16 format-name/id pairs the retail initializer at 0x1005e570 built. The names are the
// DefaultTextureFormat values, the ids the DFormatId each one selects.
struct FormatName {
    const char* name;
    int id;
};
extern const FormatName kFormatNames[16];
extern const char kDefaultFormatName[];

// Constructs the eleven shared texture names once the PString cache exists. Safe to call from any thread.
void EnsureSharedNames();

Texture*& SharedTextureSlot(int id);
const PString& SharedTextureName(int id);

}  // namespace TextureData
}  // namespace Jet
