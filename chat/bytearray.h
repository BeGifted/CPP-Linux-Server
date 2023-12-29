#ifndef __CHAT_BYTEARRAY_H__
#define __CHAT_BYTEARRAY_H__

#include <memory>
#include <string>
#include <stdint.h>
#include <vector>
#include <sys/uio.h>

namespace chat {

class ByteArray{
public:
    typedef std::shared_ptr<ByteArray> ptr;

    struct Node {  //内存池
        Node(size_t s);
        Node();
        ~Node();
        void free();

        char* ptr;
        size_t size;
        Node* next;
    };

    ByteArray(size_t base_size = 4096);
    ByteArray(void* data, size_t size, bool owner = false);
    ~ByteArray();

    bool isLittleEndian() const;
    void setIsLittleEndian(bool val);

    //write
    void writeFint8(int8_t v);
    void writeFuint8(uint8_t v);
    void writeFint16(int16_t v);
    void writeFuint16(uint16_t v);
    void writeFint32(int32_t v);
    void writeFuint32(uint32_t v);
    void writeFint64(int64_t v);
    void writeFuint64(uint64_t v);

    void writeInt32(int32_t v);
    void writeUint32(uint32_t v);
    void writeInt64(int64_t v);
    void writeUint64(uint64_t v);

    void writeFloat(float v);
    void writeDouble(double v);

    void writeStringF16(const std::string& v);
    void writeStringF32(const std::string& v);
    void writeStringF64(const std::string& v);
    void writeStringVint(const std::string& v);  //varint
    void writeStringWithoutLength(const std::string& v);

    //read
    int8_t readFint8();
    uint8_t readFuint8();
    int16_t readFint16();
    uint16_t readFuint16();
    int32_t readFint32();
    uint32_t readFuint32();
    int64_t readFint64();
    uint64_t readFuint64();

    int32_t readInt32();
    uint32_t readUint32();
    int64_t readInt64();
    uint64_t readUint64();

    float readFloat();
    double readDouble();

    std::string readStringF16();
    std::string readStringF32();
    std::string readStringF64();
    std::string readStringVint();

    //op
    void clear();
    void write(const void* buf, size_t size);
    void read(void* buf, size_t size);
    void read(void* buf, size_t size, size_t position) const;

    size_t getPosition() const {return m_position;}
    void setPosition(size_t v);

    bool writeToFile(const std::string& name) const;
    bool readFromFile(const std::string& name);

    size_t getBaseSize() const {return m_baseSize;}
    size_t getReadSize() const {return m_size - m_position;}

    std::string toString() const;
    std::string toHexString() const;
    uint64_t getReadBuffers(std::vector<iovec>& buffers, uint64_t len) const;
    uint64_t getReadBuffers(std::vector<iovec>& buffers, uint64_t len, uint64_t position) const;
    uint64_t getWriteBuffers(std::vector<iovec>& buffers, uint64_t len);
    size_t getSize() const { return m_size;}
private:
    void addCapacity(size_t size);
    size_t getCapacity() const {return m_capacity - m_position;}
private:
    size_t m_baseSize;  //内存块大小
    size_t m_position;
    size_t m_capacity;  //总容量
    size_t m_size;  //数据大小
    int8_t m_endian;  //默认大端
    Node* m_root;
    Node* m_cur;
    // 是否拥有数据的管理权限
    bool m_owner;
};

}



#endif