#ifndef _INTERFACE_PROTOCOL_H
#define _INTERFACE_PROTOCOL_H

#ifdef __cplusplus
extern "C"
{
#endif

extern char mac[23];
extern int localport;

void DealwithSerialData(uint8_t *data);
void DealwithUploadData(uint8_t *data);

#ifdef __cplusplus
}
#endif

#endif /* _INTERFACE_PROTOCOL_H */
