void check_thread_support(int provided);
void inicjuj(int *argc, char ***argv);
void finalizuj(void);
std::string returnTypeString(int type);
void sendPacket(packet_t *data, int dst, int type);
void sendToAllProcesses(packet_t *data, int type);
