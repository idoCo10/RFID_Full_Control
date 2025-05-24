/**
 * Typical pin layout used:
 * -----------------------------------------------------------------------------------------
 *             MFRC522      Arduino       Arduino   Arduino    Arduino          Arduino
 *             Reader/PCD   Uno/101       Mega      Nano v3    Leonardo/Micro   Pro Micro
 * Signal      Pin          Pin           Pin       Pin        Pin              Pin
 * -----------------------------------------------------------------------------------------
 * RST/Reset   RST          9             5         D9         RESET/ICSP-5     RST
 * SPI SS      SDA(SS)      10            53        D10        10               10
 * SPI MOSI    MOSI         11 / ICSP-4   51        D11        ICSP-4           16
 * SPI MISO    MISO         12 / ICSP-1   50        D12        ICSP-1           14
 * SPI SCK     SCK          13 / ICSP-3   52        D13        ICSP-3           15
 * This code based on this github: https://github.com/miguelbalboa/rfid/blob/master/examples/ChangeUID/ChangeUID.ino
 */

#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN   9
#define SDA_PIN   10

MFRC522 mfrc522(SDA_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

byte sourceUid[4];
char userChoice = 0;

void setup() {
  Serial.begin(9600);
  while (!Serial);
  SPI.begin();
  mfrc522.PCD_Init();

  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  Serial.println(F("\nSelect an option:"));
  Serial.println(F("1. Clone UID from one card to another"));
  Serial.println(F("2. Dump card information"));
  Serial.println(F("3. Write data to a specific block"));

  while (userChoice != '1' && userChoice != '2' && userChoice != '3') {
    if (Serial.available()) {
      userChoice = Serial.read();
    }
  }

  if (userChoice == '1') {
    cloneUID();
  } else if (userChoice == '2') {
    dumpCardInfo();
  } else if (userChoice == '3') {
    writeToCardBlock();
  }
}

void loop() {}

void cloneUID() {
  Serial.println(F("\nPlace the source card to read UID:"));
  while (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) delay(50);

  Serial.print(F("Source Card UID: "));
  for (byte i = 0; i < 4; i++) {
    sourceUid[i] = mfrc522.uid.uidByte[i];
    Serial.print(sourceUid[i] < 0x10 ? " 0" : " ");
    Serial.print(sourceUid[i], HEX);
  }
  Serial.println();
  mfrc522.PICC_HaltA();
  delay(1000);

  Serial.println(F("\nPlace the destination (magic) card:"));
  while (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) delay(50);

  Serial.print(F("Destination Card UID before change: "));
  for (byte i = 0; i < 4; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
  }
  Serial.println();

  if (mfrc522.MIFARE_SetUid(sourceUid, (byte)4, true)) {
    Serial.println(F("UID successfully cloned.\n"));
  } else {
    Serial.println(F("Failed to write UID.\n"));
  }

  mfrc522.PICC_HaltA();
  delay(1000);

  Serial.println(F("Scan the card again to verify UID:"));
  while (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) delay(50);

  Serial.print(F("Card UID after change: "));
  for (byte i = 0; i < 4; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
  }
  Serial.println();

  Serial.println(F("\nFinal Card Dump:"));
  mfrc522.PICC_DumpToSerial(&(mfrc522.uid));
  while (true);
}

void dumpCardInfo() {
  Serial.println(F("\nPlace a card to dump its data:"));
  while (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) delay(50);

  mfrc522.PICC_DumpToSerial(&(mfrc522.uid));
  Serial.println(F("Done."));
  while (true);
}

void writeToCardBlock() {

  Serial.println(F("\nCheck the card Data and available Blocks first."));
  while (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) delay(50);
  mfrc522.PICC_DumpToSerial(&(mfrc522.uid));

  Serial.println(F("\nEnter block number to write to (0–63, except trailer blocks like 3, 7, 11, etc.):"));
  while (!Serial.available());
  int block = Serial.parseInt();

  // Check if block is a trailer block
  if ((block + 1) % 4 == 0) {
    Serial.print(F("Block "));
    Serial.print(block);
    Serial.println(F(" is a trailer block. Cannot write to trailer blocks."));
    return;
  }

  // Ask user for the data
  Serial.println(F("Enter up to 16 characters to write to the block:"));
  while (!Serial.available()); // Wait for user input

  String input = Serial.readStringUntil('\n'); // Read user input
  input.trim(); // Remove any newline or extra spaces

  // Convert to byte array and pad with 0s if necessary
  byte blockcontent[16];
  int len = input.length();
  if (len > 16) len = 16; // Truncate if longer than 16

  for (int i = 0; i < 16; i++) {
    if (i < len) {
      blockcontent[i] = input[i];
    } else {
      blockcontent[i] = 0x00; // Pad with null bytes
    }
  }

  Serial.print(F("\nPlace a card to write \""));
  Serial.print(input);
  Serial.print(F("\" to block "));
  Serial.print(block);
  Serial.println(F(":"));
  while (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) delay(50);

  writeAndReadRFIDBlock(block, blockcontent);

  Serial.println(F("\nCheck the card Data."));
  dumpCardInfo();
}





void writeAndReadRFIDBlock(byte block, byte *dataToWrite) {
  if ((block + 1) % 4 == 0) {
    Serial.print(block);
    Serial.println(F(" is a trailer block. Aborting write."));
    return;
  }

  // Determine trailer block
  int trailerBlock = (block / 4) * 4 + 3;

  MFRC522::StatusCode status;

  // Authenticate
  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("Authentication failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }

  // Write
  status = mfrc522.MIFARE_Write(block, dataToWrite, 16);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("Write failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  } else {
    Serial.println(F("Write successful!"));
  }

  // Read back
  byte readbackblock[18];
  byte bufferSize = 18;

  status = mfrc522.MIFARE_Read(block, readbackblock, &bufferSize);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("Read failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }


  Serial.print(F("Read block "));
  Serial.print(block);
  Serial.print(F(": "));
  for (int i = 0; i < 16; i++) {
    Serial.write(readbackblock[i]);
  }
  Serial.println();

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}


