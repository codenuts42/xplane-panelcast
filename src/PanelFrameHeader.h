#pragma pack(push, 1)
struct PanelFrameHeader {
	uint32_t magic = 0xABCD1234;
	uint32_t frameID;
	uint16_t panelID;
	uint16_t width;
	uint16_t height;
	uint32_t compSize;
};
#pragma pack(pop)
