#include "common.h"

/*Function templates. TODO*/

void done_one(struct input_manager *in, int tnum)
{

	pthread_mutex_lock(&in->lock);  
	in->being_processed[tnum-1] = NULL;
	pthread_cond_broadcast(&in->cond);
	pthread_mutex_unlock(&in->lock);
	return;
}

int read_op(struct input_manager *in, op_t *op, int tnum)
{
	unsigned size = sizeof(op_t);
	pthread_mutex_lock(&in->lock);  
	memcpy(op, in->curr, size - sizeof(unsigned long)); //Copy till data ptr
	if (op->op_type == GET || op->op_type == DEL)
	{
		in->curr += size - sizeof(op->datalen) - sizeof(op->data);
	}
	else if (op->op_type == PUT)
	{
		in->curr += size - sizeof(op->data);
		op->data = in->curr;
		in->curr += op->datalen;
	}
	else
	{
		assert(0);
	}
	if (in->curr > in->data + in->size)
	{
		pthread_mutex_unlock(&in->lock);
		return -1;
	}
	in->being_processed[tnum-1] = op;
	for(int i=0;i<THREADS;i++)
	{
		if(i!=(tnum-1) && in->being_processed[i] && op->key == in->being_processed[i]->key && in->being_processed[i]->id < op->id && !(in->being_processed[i]->op_type == GET && op->op_type == GET))
		{
			pthread_cond_wait(&in->cond, &in->lock);
			i=-1;
		}
	}
	pthread_mutex_unlock(&in->lock);
	return 0;
// Failed
}

int lookup(hash_t *h, op_t *op)
{
	unsigned ctr;
	unsigned hashval = hashfunc(op->key, h->table_size);
	hash_entry_t *entry = h->table + hashval;
	int start =1 ;
	ctr = hashval;
	while(1){
		pthread_mutex_lock(&entry->lock);
		if((entry->key || entry->id == (unsigned)-1) &&
		   entry->key != op->key && (start || ctr != hashval))
		{
			start=0;
		    ctr = (ctr + 1) % h->table_size;
		    pthread_mutex_unlock(&entry->lock);
		    entry = h->table + ctr;
		}
		else{
			if (entry->key == op->key)
			{
				op->datalen = entry->datalen;
				op->data = entry->data;
				pthread_mutex_unlock(&entry->lock);
				return 0;
			}
			break;
		}
	}
	pthread_mutex_unlock(&entry->lock);
	return -1;
}

int insert_update(hash_t *h, op_t *op)
{
	unsigned ctr;
	unsigned hashval = hashfunc(op->key, h->table_size);
	hash_entry_t *entry = h->table + hashval;

	assert(h && h->used <= h->table_size);
	assert(op && op->key);

	ctr = hashval;
	int start =1 ;
	int special_cond = 0;
	while(1){
		pthread_mutex_lock(&entry->lock);
		if((entry->key || entry->id == (unsigned)(-1)) && entry->key != op->key && (start || ctr != hashval))
		{
			start=0;
			ctr = (ctr + 1) % h->table_size;
			pthread_mutex_unlock(&entry->lock);
			entry = h->table + ctr;
		}
		else{
			// assert(ctr != hashval - 1);
			if(start==0 && ctr == hashval){
				special_cond=1;
				pthread_mutex_unlock(&entry->lock);
				break;
			}
			if (entry->key == op->key)
			{ //It is an update
				entry->id = op->id;
				entry->datalen = op->datalen;
				entry->data = op->data;
				pthread_mutex_unlock(&entry->lock);
				return 0;
			}
			else{
				assert(!entry->key);
				//its a new insert
				entry->id = op->id;
				entry->datalen = op->datalen;
				entry->key = op->key;
				entry->data = op->data;
				// atomic_add(&h->used, 1);   //try
				__sync_fetch_and_add(&h->used, 1);
				pthread_mutex_unlock(&entry->lock);
				
				return 0;
			}
		}
	}
	if(special_cond){
		// printf("Harsh");
		start=1;
		while(1){
			pthread_mutex_lock(&entry->lock);
			if((entry->key) && entry->key != op->key && (start || ctr != hashval))
			{
				start=0;
				ctr = (ctr + 1) % h->table_size;
				pthread_mutex_unlock(&entry->lock);
				entry = h->table + ctr;
			}
			else{
				if(start==0 && ctr == hashval){
					pthread_mutex_unlock(&entry->lock);
					return -1;
				}
				if(entry->id == (unsigned)-1)    ///since update is not possible in special case. There can be only insertion at first (-1)position.
				{
					entry->id = op->id;
					entry->datalen = op->datalen;
					entry->key = op->key;
					entry->data = op->data;
					// atomic_add(&h->used, 1);   //try
					__sync_fetch_and_add(&h->used, 1);
					pthread_mutex_unlock(&entry->lock);
					return 0;
				}
				break;
			}
		}
	}
	pthread_mutex_unlock(&entry->lock);
	return -1;
}

int purge_key(hash_t *h, op_t *op)
{
	unsigned ctr;
	unsigned hashval = hashfunc(op->key, h->table_size);
	hash_entry_t *entry = h->table + hashval;

	ctr = hashval;
	int start=1;
	while(1){
		pthread_mutex_lock(&entry->lock);
		if((entry->key || entry->id == (unsigned)-1) &&
		   entry->key != op->key &&(start || ctr != hashval))
		{
			start=0;
			ctr = (ctr + 1) % h->table_size;
			pthread_mutex_unlock(&entry->lock);
			entry = h->table + ctr;
		}
		else{
			if (entry->key == op->key)
			{							  //Found. purge it
				entry->id = (unsigned)-1; //Empty but deleted
				entry->key = 0;
				entry->datalen = 0;
				entry->data = NULL;
				// atomic_add(&h->used, -1);
				__sync_fetch_and_sub(&h->used, 1);
				pthread_mutex_unlock(&entry->lock);
				
				return 0;
			}
			break;
		}
	}
	pthread_mutex_unlock(&entry->lock);
	return -1; // Bogus purge
}
