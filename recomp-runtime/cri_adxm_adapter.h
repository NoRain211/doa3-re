#ifndef DOA3_RECOMP_CRI_ADXM_ADAPTER_H
#define DOA3_RECOMP_CRI_ADXM_ADAPTER_H

/* DOA3 CRI ADXM service pass and cooperative main-worker entry. The inline
   pass remains for callers that poll ADXF status without a scheduling point. */
void recomp_cri_adxm_server_step(void);
void recomp_cri_adxm_main_thread(void);
void recomp_cri_adxm_vblank_a_thread(void);
void recomp_cri_adxm_vblank_b_thread(void);

#endif
