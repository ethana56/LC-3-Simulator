#ifndef INPUT_OUTPUT_H
#define INPUT_OUTPUT_H

struct std_input_output;
typedef struct std_input_output StdInputOutput;

StdInputOutput *grab_std_input_output(int *);
void give_back_std_input_output(StdInputOutput *);
int stdinputoutput_device_write_display(StdInputOutput *, unsigned char);
int stdinputoutput_device_read_keyboard(StdInputOutput *);

#endif
