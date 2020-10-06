# Light-Tracking-on-Solar-Panel
Embedded System project: designed photoresistors on solar panel with servomotor to track the light source. Find the useful templates from TI microcontroller library to utillize HWI, SWI and TSK threads.

video: https://www.youtube.com/embed/GdUEGCkWs9c

No floating-point datatypes – I compared integer values(less processing time) for direction turning condition rather than floats which causes the servomotor movement wiggles so much.

My algorithm of digital signal processing - I set the timer a little longer to have a relatively stable movement when tracking the light. When compared with the servomotor movement direction, I compared the percentages by reading 20-50 samples of each photo resistors’ value and average the results which has the average max range.



 
