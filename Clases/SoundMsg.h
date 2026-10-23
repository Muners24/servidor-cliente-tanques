#include "../t.h"

typedef struct _soundmsg
{
    int id;
    Vector2 origen;
} _SoundMsg;

class SoundMsg
{
private:
    Vector2 origen;
    int id;
    int timer;
public:
    map<int, bool> send;

    SoundMsg(int id, list<int> clients_id, Vector2 origen);
    SoundMsg(){};

    bool operator==(const SoundMsg &t) const;
    Vector2 getOrg() { return origen; };
    bool shouldPlay(int id_client);
    ~SoundMsg() { send.clear(); };
    int getId() { return id; };
    bool readyToRemove();
    _SoundMsg toStruct();
};