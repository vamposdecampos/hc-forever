-- Top-level module for a GODIL_XC3S500E from OHO-Elektronik
-- Signal names are the ones used by OHO in their reference design

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity top_fpga_vidgen is
port (
	m49, sw1, sw2, sout, rts, c13, d13, tdin:	in  std_logic;
	cts, vs2, sin:					out std_logic;
	tvs1, tvs0, tmosi, tcclk, cso:			out std_logic;
	tm1, thsw:					inout std_logic;
	pin:						inout std_logic_vector(48 downto 1)
);
end top_fpga_vidgen;


architecture beh of top_fpga_vidgen is

signal Clock7		: std_logic;
signal Clock3p5		: std_logic;
signal CpuClock		: std_logic;
signal Tick1us		: std_logic;
signal TickCount	: unsigned(2 downto 0);
signal SysReset		: std_logic;
signal SysReset_n	: std_logic;

signal Carry		: std_logic := '0';
signal Blank		: std_logic := '0';
signal Highlight	: std_logic := '0';
signal HighlightPin	: std_logic := '0';
signal Sync		: std_logic := '0';
signal Red		: std_logic := '0';
signal Green		: std_logic := '0';
signal Blue		: std_logic := '0';
signal FlashCount	: unsigned(4 downto 0) := (others => '0');
signal BorderReg	: std_logic_vector(2 downto 0);

signal VideoAddress	: std_logic_vector(13 downto 0);
signal VideoDataEn	: std_logic := '0';
signal VideoData	: std_logic_vector(7 downto 0);

signal vram_addr	: std_logic_vector(15 downto 0);
signal mem_addr		: std_logic_vector(15 downto 0);
signal mem_din		: std_logic_vector(7 downto 0);
signal mem_dout		: std_logic_vector(7 downto 0);
signal mem_sel		: std_logic;
signal mem_wr		: std_logic;
signal rom_sel		: std_logic;
signal ram_sel		: std_logic;
signal xmem_sel		: std_logic;
signal portfe_sel	: std_logic;
signal portfe_din	: std_logic_vector(7 downto 0) := (others => '1');

signal jtag_din		: std_logic_vector(31 downto 0);
signal jtag_we		: std_logic;
signal jtag_addr	: std_logic_vector(15 downto 0);
signal jtag_data	: std_logic_vector(7 downto 0);

signal cpu_mreq_n	: std_logic;
signal cpu_iorq_n	: std_logic;
signal cpu_rd_n		: std_logic;
signal cpu_wr_n		: std_logic;
signal cpu_int_n	: std_logic;
signal cpu_wait_n	: std_logic;
signal cpu_busrq_n	: std_logic;
signal cpu_busak_n	: std_logic;
signal cpu_addr		: std_logic_vector(15 downto 0);
signal cpu_din		: std_logic_vector(7 downto 0);
signal cpu_dout		: std_logic_vector(7 downto 0);

signal cpu_mreq		: std_logic;
signal cpu_iorq		: std_logic;
signal cpu_rd		: std_logic;
signal cpu_wr		: std_logic;
signal cpu_int		: std_logic;
signal cpu_wait		: std_logic;
signal cpu_busrq	: std_logic;

signal xmem_din		: std_logic_vector(7 downto 0);
signal xmem_dout	: std_logic_vector(7 downto 0);
signal xmem_addr	: std_logic_vector(16 downto 0);
signal xmem_cs		: std_logic;
signal xmem_we		: std_logic;
signal xmem_oe		: std_logic;

signal spim_sel_data	: std_logic;
signal spim_sel_ctrl	: std_logic;
signal spim_wr		: std_logic;
signal spim_dout	: std_logic_vector(7 downto 0);
signal spim_cs		: std_logic_vector(2 downto 0);

begin

	SysReset_n <= sw2; -- sw2 is active low
	SysReset <= not SysReset_n;

	clocking: entity work.GodilClocking
		port map (
			BoardClock	=> m49,
			Clock7		=> Clock7,
			Clock3p5	=> Clock3p5,
			CpuClock	=> CpuClock
		);

	vidgen: entity work.VideoGen
		port map (
			Clock7		=> Clock7,
			Clock3p5	=> Clock3p5,
			VideoAddress	=> VideoAddress,
			VideoData	=> VideoData,
			VideoDataEn	=> VideoDataEn,
			VideoBusReq	=> cpu_wait,
			FrameInterrupt	=> cpu_int,
			FrameCarry	=> Carry,
			Red		=> Red,
			Green		=> Green,
			Blue		=> Blue,
			Highlight	=> Highlight,
			Blank		=> Blank,
			Sync		=> Sync,
			BorderRed	=> BorderReg(1),
			BorderGreen	=> BorderReg(2),
			BorderBlue	=> BorderReg(0),
			FlashClock	=> FlashCount(4)
		);

	vram: entity work.VideoRam
		port map (
			Clock		=> Clock7,
			Enable		=> mem_sel,
			Address		=> mem_addr(14 downto 0),
			DataOut		=> mem_dout,
			WriteEnable	=> mem_wr,
			DataIn		=> mem_din,
			DualEnable	=> VideoDataEn,
			DualAddress	=> vram_addr(14 downto 0),
			DualDataOut	=> VideoData
		);

	bscan: entity work.BscanUser
		generic map (
			DR_LEN		=> jtag_din'length
		)
		port map (
			Clock			=> Clock7,
			DataIn			=> jtag_din,
			DataOut(31)		=> jtag_we,
			DataOut(30)		=> cpu_busrq,
			DataOut(29 downto 24)	=> open,
			DataOut(23 downto 8)	=> jtag_addr,
			DataOut(7 downto 0)	=> jtag_data
		);

	kbd: entity work.PS2_MatrixEncoder
		port map (
			Clk			=> Clock7,
			Reset_n			=> SysReset_n,
			Tick1us			=> Tick1us,
			PS2_Clk			=> pin(47),
			PS2_Data		=> pin(48),
			Key_Addr		=> cpu_addr(15 downto 8),
			Key_Data		=> portfe_din(4 downto 0)
		);

	spi_master: entity work.SpiMaster
		port map (
			Clock			=> Clock7,
			Reset			=> SysReset,
			WriteEnable		=> spim_wr,
			ControlSelect		=> spim_sel_ctrl,
			ChipSelect		=> spim_cs,
			DataIn			=> cpu_dout,
			DataOut			=> spim_dout,
			SpiMiso			=> tdin,
			SpiMosi			=> tmosi,
			SpiClock		=> tcclk
		);

	z80: entity work.T80s port map (
		RESET_n => SysReset_n,
		CLK_n => CpuClock,
		WAIT_n => '1', --cpu_wait_n,
		INT_n => cpu_int_n,
		NMI_n => '1',
		BUSRQ_n => cpu_busrq_n,
		M1_n => open,
		MREQ_n => cpu_mreq_n,
		IORQ_n => cpu_iorq_n,
		RD_n => cpu_rd_n,
		WR_n => cpu_wr_n,
		RFSH_n => open,
		HALT_n => cts,
		BUSAK_n => cpu_busak_n,
		A => cpu_addr,
		DI => cpu_din,
		DO => cpu_dout
	);

	cpu_rd <= not cpu_rd_n;
	cpu_wr <= not cpu_wr_n;
	cpu_mreq <= not cpu_mreq_n;
	cpu_iorq <= not cpu_iorq_n;
	cpu_int_n <= not cpu_int;
	cpu_wait_n <= not cpu_wait;
	cpu_busrq_n <= not cpu_busrq;

	spim_sel_ctrl <= '1' when 
		cpu_iorq = '1' and cpu_addr(7 downto 0) = x"1F"
		else '0';
	spim_sel_data <= '1' when 
		cpu_iorq = '1' and cpu_addr(7 downto 0) = x"3F"
		else '0';
	spim_wr <= (spim_sel_ctrl or spim_sel_data) and cpu_wr;

	-- video ram & jtag
	mem_sel <= cpu_mreq or jtag_we;
	rom_sel <= not cpu_addr(15) and not cpu_addr(14);
	ram_sel <= not cpu_addr(15) and cpu_addr(14); -- XXX 16k hack
	xmem_sel <= cpu_addr(15);
	mem_wr <= (cpu_mreq and cpu_wr and ram_sel) or jtag_we;
	mem_addr <= jtag_addr when jtag_we = '1' else cpu_addr;
	mem_din <= jtag_data when jtag_we = '1' else cpu_dout;
	xmem_din <= cpu_dout;
	xmem_oe <= xmem_sel and cpu_mreq and cpu_rd;
	xmem_we <= xmem_sel and cpu_mreq and cpu_wr;
	xmem_cs <= xmem_sel;
	cpu_din <= mem_dout when cpu_mreq = '1' and (rom_sel = '1' or ram_sel = '1') else
		xmem_dout when cpu_mreq = '1' and xmem_sel = '1' else
		portfe_din when portfe_sel = '1' else
		spim_dout when spim_sel_data = '1' else
		VideoData when VideoDataEn = '1' else
		x"FF";

	vram_addr <= "01" & VideoAddress;
	xmem_addr <= "00" & cpu_addr(14 downto 0);

	jtag_din <= (0 => sw1, 1 => sw2, 3 => cpu_busak_n, others => '0');

	-- port FEh
	portfe_sel <= cpu_iorq and not cpu_addr(0);
	portfe_din(5) <= pin(46);	-- unused
	portfe_din(6) <= pin(45);	-- EAR input
	portfe_din(7) <= pin(44);	-- unused

	process (Clock7)
	begin
		if rising_edge(Clock7) then
			if portfe_sel = '1' and cpu_wr = '1' then
				BorderReg <= cpu_dout(2 downto 0);
				pin(7) <= cpu_dout(3); -- MIC
				pin(8) <= cpu_dout(4); -- EAR
			end if;
		end if;
	end process;

	-- flash
	process (Clock7)
	begin
		if rising_edge(Clock7) and Carry = '1' then
			FlashCount <= FlashCount + 1;
		end if;
	end process;

	-- 1us tick
	process (Clock7)
	begin
		if rising_edge(Clock7) then
			if TickCount = "110" then
				TickCount <= (others => '0');
			else
				TickCount <= TickCount + 1;
			end if;
		end if;
	end process;
	Tick1us <= '1' when TickCount = "000" else '0';

	-- black is not highlighted on the Spectrum
	HighlightPin <= Highlight and (Red or Green or Blue);

	-- 5-bit passive DAC
	pin(6 downto 2) <=
		"00000" when Sync = '1' else
		"10000" when Blank = '1' else
		"1" & HighlightPin & Green & Red & Blue;

	-- external memory (621024)
	pin(10) <= xmem_addr(16);
	pin(11) <= xmem_addr(14);
	pin(12) <= xmem_addr(12);
	pin(13) <= xmem_addr(7);
	pin(14) <= xmem_addr(6);
	pin(15) <= xmem_addr(5);
	pin(16) <= xmem_addr(4);
	pin(17) <= xmem_addr(3);
	pin(18) <= xmem_addr(2);
	pin(19) <= xmem_addr(1);
	pin(20) <= xmem_addr(0);
	pin(31) <= xmem_addr(10);
	pin(33) <= xmem_addr(11);
	pin(34) <= xmem_addr(9);
	pin(35) <= xmem_addr(8);
	pin(36) <= xmem_addr(13);
	pin(39) <= xmem_addr(15);
	pin(30) <= not xmem_cs;
	pin(32) <= not xmem_oe;
	pin(37) <= not xmem_we;
	xmem_dout(0) <= pin(21);
	xmem_dout(1) <= pin(22);
	xmem_dout(2) <= pin(23);
	xmem_dout(3) <= pin(25);
	xmem_dout(4) <= pin(26);
	xmem_dout(5) <= pin(27);
	xmem_dout(6) <= pin(28);
	xmem_dout(7) <= pin(29);
	pin(21) <= xmem_din(0) when xmem_we = '1' else 'Z';
	pin(22) <= xmem_din(1) when xmem_we = '1' else 'Z';
	pin(23) <= xmem_din(2) when xmem_we = '1' else 'Z';
	pin(25) <= xmem_din(3) when xmem_we = '1' else 'Z';
	pin(26) <= xmem_din(4) when xmem_we = '1' else 'Z';
	pin(27) <= xmem_din(5) when xmem_we = '1' else 'Z';
	pin(28) <= xmem_din(6) when xmem_we = '1' else 'Z';
	pin(29) <= xmem_din(7) when xmem_we = '1' else 'Z';

	pin(1) <= 'Z';
	pin(9) <= 'Z';
	pin(24) <= 'Z';
	pin(38) <= 'Z';
	pin(48 downto 40) <= (others => 'Z');
	--pin(48 downto 32) <= (others => 'Z');
	--pin(30 downto 9) <= (others => 'Z');

	tvs0 <= not spim_cs(0);
	tvs1 <= not spim_cs(1);
	cso <= not spim_cs(2);
	tm1 <= 'Z';
	thsw <= 'Z';

	vs2 <= FlashCount(4);
	--sin <= sout xor sw1 xor sw2 xor rts xor c13 xor d13;
	sin <= tdin;

end beh;
