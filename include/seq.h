/****************************************************************************
 *                                                                          *
 *                                seq.h                                     *
 *                              Sequencer                                   *
 *                                                                          *
 ****************************************************************************/

#define SND_SEQ_OPEN_OUT	(O_WRONLY)
#define SND_SEQ_OPEN_IN		(O_RDONLY)
#define SND_SEQ_OPEN		(O_RDWR)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct snd_seq snd_seq_t;

int snd_seq_open(snd_seq_t **handle, int mode);
int snd_seq_close(snd_seq_t *handle);
int snd_seq_file_descriptor(snd_seq_t *handle);
int snd_seq_block_mode(snd_seq_t *handle, int enable);
int snd_seq_client_id(snd_seq_t *handle);
int snd_seq_system_info(snd_seq_t *handle, snd_seq_system_info_t *info);
int snd_seq_get_client_info(snd_seq_t *handle, snd_seq_client_info_t *info);
int snd_seq_get_any_client_info(snd_seq_t *handle, int client, snd_seq_client_info_t *info);
int snd_seq_set_client_info(snd_seq_t *handle, snd_seq_client_info_t *info);
int snd_seq_create_port(snd_seq_t *handle, snd_seq_port_info_t *info);
int snd_seq_delete_port(snd_seq_t *handle, snd_seq_port_info_t *info);
int snd_seq_get_port_info(snd_seq_t *handle, int port, snd_seq_port_info_t *info);
int snd_seq_get_any_port_info(snd_seq_t *handle, int client, int port, snd_seq_port_info_t *info);
int snd_seq_set_port_info(snd_seq_t *handle, int port, snd_seq_port_info_t *info);
int snd_seq_get_port_subscription(snd_seq_t *handle, snd_seq_port_subscribe_t *sub);
int snd_seq_subscribe_port(snd_seq_t *handle, snd_seq_port_subscribe_t *sub);
int snd_seq_unsubscribe_port(snd_seq_t *handle, snd_seq_port_subscribe_t *sub);
int snd_seq_query_port_subscribers(snd_seq_t *seq, snd_seq_query_subs_t * subs);
int snd_seq_get_queue_status(snd_seq_t *handle, int q, snd_seq_queue_status_t *status);
int snd_seq_get_queue_tempo(snd_seq_t *handle, int q, snd_seq_queue_tempo_t *tempo);
int snd_seq_set_queue_tempo(snd_seq_t *handle, int q, snd_seq_queue_tempo_t *tempo);
int snd_seq_get_queue_owner(snd_seq_t *handle, int q, snd_seq_queue_owner_t *owner);
int snd_seq_set_queue_owner(snd_seq_t *handle, int q, snd_seq_queue_owner_t *owner);
int snd_seq_get_queue_timer(snd_seq_t *handle, int q, snd_seq_queue_timer_t *timer);
int snd_seq_set_queue_timer(snd_seq_t *handle, int q, snd_seq_queue_timer_t *timer);
int snd_seq_get_queue_sync(snd_seq_t *handle, int q, snd_seq_queue_sync_t *sync);
int snd_seq_set_queue_sync(snd_seq_t *handle, int q, snd_seq_queue_sync_t *sync);
int snd_seq_get_queue_client(snd_seq_t *handle, int q, snd_seq_queue_client_t *queue);
int snd_seq_set_queue_client(snd_seq_t *handle, int q, snd_seq_queue_client_t *queue);
int snd_seq_alloc_named_queue(snd_seq_t *seq, char *name);
int snd_seq_alloc_queue(snd_seq_t *handle);
int snd_seq_free_queue(snd_seq_t *handle, int q);
int snd_seq_get_queue_info(snd_seq_t *seq, int q, snd_seq_queue_info_t *info);
int snd_seq_set_queue_info(snd_seq_t *seq, int q, snd_seq_queue_info_t *info);
int snd_seq_get_named_queue(snd_seq_t *seq, char *name);
int snd_seq_get_client_pool(snd_seq_t *handle, snd_seq_client_pool_t * info);
int snd_seq_set_client_pool(snd_seq_t *handle, snd_seq_client_pool_t * info);
int snd_seq_query_next_client(snd_seq_t *handle, snd_seq_client_info_t * info);
int snd_seq_query_next_port(snd_seq_t *handle, snd_seq_port_info_t * info);

/* event routines */
snd_seq_event_t *snd_seq_create_event(void);
int snd_seq_free_event(snd_seq_event_t *ev);
int snd_seq_event_length(snd_seq_event_t *ev);
int snd_seq_event_output(snd_seq_t *handle, snd_seq_event_t *ev);
int snd_seq_event_input(snd_seq_t *handle, snd_seq_event_t **ev);
int snd_seq_event_input_pending(snd_seq_t *seq, int fetch_sequencer);
int snd_seq_event_input_selective(snd_seq_t *seq, snd_seq_event_t **ev, int type, int blocking);
int snd_seq_flush_output(snd_seq_t *handle);
int snd_seq_event_output_pending(snd_seq_t *seq);
int snd_seq_extract_output(snd_seq_t *handle, snd_seq_event_t **ev);
int snd_seq_drain_output(snd_seq_t *handle);
int snd_seq_drain_output_buffer(snd_seq_t *handle);
int snd_seq_drain_input(snd_seq_t *handle);
int snd_seq_drain_input_buffer(snd_seq_t *handle);
int snd_seq_remove_events(snd_seq_t *handle, snd_seq_remove_events_t *info);
/* misc */
void snd_seq_set_bit(int nr, void *array);
int snd_seq_change_bit(int nr, void *array);
int snd_seq_get_bit(int nr, void *array);

#ifdef __cplusplus
}
#endif

