module CORELOAD
(
	input wire       CLK  ,
	input wire      nRESET,
	input wire       CS   ,
	input wire [7:0] DIN
);

	reg [23:0] ADDRESS = 24'h080000;
	reg        STROBE  =  1'b0     ;
 
	always @(posedge CLK)
		begin
			if (nRESET == 1'b0) STROBE <= 1'b0;
			else
				begin
					if (CS == 1'b1)
						begin
							ADDRESS <= {DIN[4:0], 19'b0000000000000000000};
							STROBE  <= DIN[7];
						end
				end
		end

	reg [4:0] q   = 5'b00000;
	reg reboot_ff = 1'b0;

	always @(posedge CLK)
		begin
			q[0] <= STROBE;
			q[1] <= q[0];
			q[2] <= q[1];
			q[3] <= q[2];
			q[4] <= q[3];
			reboot_ff <= (q[4] && (!q[3]) && (!q[2]) && (!q[1]));
		end

	CORELOAD_SPARTAN6 FPGA_CORELOAD
	(
		.CLK       (CLK      ),
		.MBT_RESET (1'b0     ),
		.MBT_REBOOT(reboot_ff),
		.ADDRESS   (ADDRESS  )
	);

endmodule            

module CORELOAD_SPARTAN6
(
	input wire        CLK       ,
	input wire        MBT_RESET ,
	input wire        MBT_REBOOT,
	input wire [23:0] ADDRESS
);

	reg [15:0] icap_din;
	reg        icap_ce ;
	reg        icap_wr ;

	reg [15:0] ff_icap_din_reversed;
	reg        ff_icap_ce          ;
	reg        ff_icap_wr          ;

	ICAP_SPARTAN6 ICAP_SPARTAN6_inst
	(
		.CE    (ff_icap_ce)          , // Clock enable input
		.CLK   (CLK)                 , // Clock input
		.I     (ff_icap_din_reversed), // 16-bit data input
		.WRITE (ff_icap_wr)            // Write input
	);

//	STATE MACHINE

	parameter IDLE   =  0,
				 SYNC_H =  1,
				 SYNC_L =  2,
				 CWD_H  =  3,
				 CWD_L  =  4,
				 GEN1_H =  5,
				 GEN1_L =  6,
				 GEN2_H =  7,
				 GEN2_L =  8,
				 GEN3_H =  9,
				 GEN3_L = 10,
				 GEN4_H = 11,
				 GEN4_L = 12,
				 GEN5_H = 13,
				 GEN5_L = 14,
				 NUL_H  = 15,
				 NUL_L  = 16,
				 MOD_H  = 17,
				 MOD_L  = 18,
				 HCO_H  = 19,
				 HCO_L  = 20,
				 RBT_H  = 21,
				 RBT_L  = 22,
				 NOOP_0 = 23,
				 NOOP_1 = 24,
				 NOOP_2 = 25,
				 NOOP_3 = 26;

	reg [4:0]      state;
	reg [4:0] next_state;

	always @*
		begin: COMB
			case (state)
				IDLE:
					begin
						if (MBT_REBOOT)
							begin
								next_state = SYNC_H;
								icap_ce    = 0;
								icap_wr    = 0;
								icap_din   = 16'hAA99; // Sync word 1 
							end
						else
							begin
								next_state = IDLE;
								icap_ce    = 1;
								icap_wr    = 1;
								icap_din   = 16'hFFFF; // Null 
							end
					end
				SYNC_H:
					begin
						next_state = SYNC_L;
						icap_ce    = 0;
						icap_wr    = 0;
						icap_din   = 16'h5566; // Sync word 2
					end
				SYNC_L:
					begin
						next_state = NUL_H;
						icap_ce    = 0;
						icap_wr    = 0;
						icap_din   = 16'h30A1; // Write to Command Register
					end
				NUL_H:
					begin
						next_state = GEN1_H;
						icap_ce    = 0;
						icap_wr    = 0;
						icap_din   = 16'h0000; // Null Command
					end
				GEN1_H:
					begin
						next_state = GEN1_L;
						icap_ce    = 0;
						icap_wr    = 0;
						icap_din   = 16'h3261; // Write to GENERAL_1
					end
				GEN1_L:
					begin
						next_state = GEN2_H;
						icap_ce    = 0;
						icap_wr    = 0;
						icap_din   = ADDRESS[15:0]; // Address low word
					end
				GEN2_H:
					begin
						next_state = GEN2_L;
						icap_ce    = 0;
						icap_wr    = 0;
						icap_din   = 16'h3281; // Write to GENERAL_2
					end
				GEN2_L:
					begin
						next_state = GEN3_H;
						icap_ce    = 0;
						icap_wr    = 0;
//						icap_din   = {8'h6B, ADDRESS[23:16]}; // Read 4X opcode + Address high byte
						icap_din   = {8'h03, ADDRESS[23:16]}; // Read 1X opcode + Address high byte
					end
				GEN3_H:
					begin
						next_state = GEN3_L;
						icap_ce    = 0;
						icap_wr    = 0;
						icap_din   = 16'h32A1; // Write to GENERAL_3
					end
				GEN3_L:
					begin
						next_state = GEN4_H;
						icap_ce    = 0;
						icap_wr    = 0;
						icap_din   = 16'h0000; // Address low word
					end
				GEN4_H:
					begin
						next_state = GEN4_L;
						icap_ce    = 0;
						icap_wr    = 0;
						icap_din   = 16'h32C1; // Write to GENERAL_4
					end
				GEN4_L:
					begin
						next_state = GEN5_H;
						icap_ce    = 0;
						icap_wr    = 0;
						icap_din   = 16'h0300; // Read 1X opcode + Address high byte
					end
				GEN5_H:
					begin
						next_state = GEN5_L;
						icap_ce    = 0;
						icap_wr    = 0;
						icap_din   = 16'h32E1; // Write to GENERAL_5
					end
				GEN5_L:
					begin
						next_state = MOD_H;
						icap_ce    = 0;
						icap_wr    = 0;
						icap_din   = 16'h0000;
					end
				MOD_H:
					begin
						next_state = MOD_L;
						icap_ce    = 0;
						icap_wr    = 0;
						icap_din   = 16'h3301; // Write to MODE
					end
				MOD_L:
					begin
						next_state = NUL_L;
						icap_ce    = 0;
						icap_wr    = 0;
//						icap_din   = 16'h3100; // 4X config read
						icap_din   = 16'h2100; // 1X config read
					end
				NUL_L:
					begin
						next_state = RBT_H;
						icap_ce    = 0;
						icap_wr    = 0;
						icap_din   = 16'h30A1; // Write to Command Register
					end
				RBT_H:
					begin
						next_state = RBT_L;
						icap_ce    = 0;
						icap_wr    = 0;
						icap_din   = 16'h000E; // REBOOT Command
					end
				RBT_L:
					begin
						next_state = NOOP_0;
						icap_ce    = 0;
						icap_wr    = 0;
						icap_din   = 16'h2000; // NOOP
					end
				NOOP_0:
					begin
						next_state = NOOP_1;
						icap_ce    = 0;
						icap_wr    = 0;
						icap_din   = 16'h2000; // NOOP
					end
				NOOP_1:
					begin
						next_state = NOOP_2;
						icap_ce    = 0;
						icap_wr    = 0;
						icap_din   = 16'h2000; // NOOP
					end
				NOOP_2:
					begin
						next_state = NOOP_3;
						icap_ce    = 0;
						icap_wr    = 0;
						icap_din   = 16'h2000; // NOOP
					end
				NOOP_3:
					begin
						next_state = IDLE;
						icap_ce    = 1;
						icap_wr    = 1;
						icap_din   = 16'h1111; // NULL
					end
				default:
					begin
						next_state = IDLE;
						icap_ce    = 1;
						icap_wr    = 1;
						icap_din   = 16'h1111; // NULL
					end
			endcase
		end

	always @(posedge CLK)
		begin: SEQ
			if (MBT_RESET) state <= IDLE      ;
			else           state <= next_state;
		end

	always @(posedge CLK)
		begin: ICAP_FF
			ff_icap_din_reversed[ 0] <= icap_din[ 7];
			ff_icap_din_reversed[ 1] <= icap_din[ 6];
			ff_icap_din_reversed[ 2] <= icap_din[ 5];
			ff_icap_din_reversed[ 3] <= icap_din[ 4];
			ff_icap_din_reversed[ 4] <= icap_din[ 3];
			ff_icap_din_reversed[ 5] <= icap_din[ 2];
			ff_icap_din_reversed[ 6] <= icap_din[ 1];
			ff_icap_din_reversed[ 7] <= icap_din[ 0];
			ff_icap_din_reversed[ 8] <= icap_din[15];
			ff_icap_din_reversed[ 9] <= icap_din[14];
			ff_icap_din_reversed[10] <= icap_din[13];
			ff_icap_din_reversed[11] <= icap_din[12];
			ff_icap_din_reversed[12] <= icap_din[11];
			ff_icap_din_reversed[13] <= icap_din[10];
			ff_icap_din_reversed[14] <= icap_din[ 9];
			ff_icap_din_reversed[15] <= icap_din[ 8];

			ff_icap_ce <= icap_ce;
			ff_icap_wr <= icap_wr;
		end

endmodule
