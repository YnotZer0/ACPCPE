"""
ACPCPE - A CPC Printer Emulator

With the provided Arduino Sketch, the Amstrad CPC's Printer Port parallel data
is translated into hexadecimal bytes that are sent from the Arduino's USB Serial 
Port to a PC running this Python program, which processes the received bytes to 
make a file that emulates a printer output.

This program is compatible with Epson Standard Code for Printers (ESC/P).

Word processors for the Amstrad CPC (e.g. Protext, Amsword, Tasword) send ESC/P codes
to make a printer (a matrix printer) to print different types of font, set the size 
of margins, jump page, etc.


How it works:
	If the bytes received contain ESC/P codes (e.g. wordprocessor text), this program 
	will generate an HTML (.html) file as output, where the ESC/P codes are substituted
	by HTML mark-up codes (e.g. <b>for bold text</b>). Markdown files can also be generated.

If the bytes received do not contain any ESC/P (e.g. a BASIC program print or plain text), 
this program will generate a simple text (.txt) file.

The communication with the Arduino program works as follows:
	- At the press of the push-button (LED goes ON to indicate ONLINE), the Arduino sends 
		the ESC/P command 17 (Select Printer) and starts listening from the Amstrad CPC Printer port.
	- At another press of the push-button (LED goes OFF to indicate OFFLINE), the Arduino sends 
		the ESC/P command 19 (Deselect Printer) and stops to listen from the Amstrad CPC Printer port.
	- This Python program uses these two commands to open and close the files.

Supported ESC/P codes so far:
	10			Line Feed
	13			Carriage Return
	17			Select printer
	19			Deselect printer
	27 64		Initialise Printer
	27 45 48	Cancel underlining
	27 45 49	Select underlining
	27 52		Select italic mode
	27 53		Cancel italic mode
	27 69		Select emphasised (bold) mode
	27 70		Cancel emphasised (bold) mode
	27 71		Select double strike mode
	27 72		Cancel double strike mode
	27 83 48	Select superscript
	27 83 49	Select subscript
	27 87		Cancel superscript/subscript

---------------------------LICENSE NOTICE-------------------------------- 
MIT License

Copyright (c) 2019 David Asta

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
"""

import sys
import serial
import time
import datetime
import argparse
from serial.tools.list_ports import comports

__version__ = "v0.1.0"

class acpcpe:
	############################################################
	# class init
	def __init__(self, port, echo, quiet, nofile, nolog, raw, markdown):
		self.port = port
		self.doEcho = echo
		self.beQuiet = quiet
		self.doFile = not nofile
		self.doLog = not nolog
		self.doRaw = raw
		self.doMarkdown = markdown
		self.baudrate = 115200
		self.ser = serial.Serial()
		self.buffer = []
		self.bufferoutput = []
		self.haveESC = False

		if(self.doMarkdown):
			self.doHTML = False
		else:
			self.doHTML = True

		if(self.doLog):
			logfile = open("ACPCPE.log", "w")
			logfile.close()

	############################################################
	# Set serial values and open port for start receiving data
	def setserial(self):
		try:
			self.ser.baudrate = self.baudrate
			self.ser.port = self.port
			self.ser.open()
			if self.doLog:
				self.log2file("ACPCPE started.")
			if(not self.beQuiet):
				print("\nACPCPE is listening now from port: " + self.ser.name)
				print("Press Ctrl+Break to stop.")
				sys.stdout.flush()
		except Exception as e:
			print("\n")
			print(e)
			print("\nAvailable ports:")
			comlist = comports()
			for port in comlist:
				print(port)
			print("\n")
			sys.stdout.flush()
			sys.exit(1)

	############################################################
	# Start listening and adding bytes to buffer array
	def listen(self):
		ibyte = 0
		rcvddata = False
		isListening = True
		printerIsOnline = False
		self.buffer.clear()

		while(isListening):
			rcvbyte = self.ser.read(2)
			ibyte = int(rcvbyte, 16)
#			print(ibyte)
#			sys.stdout.flush()

			if(printerIsOnline):
				if(ibyte == 25):
					self.log2file("Printer went offline")
					sys.stdout.flush()
					printerIsOnline = False
					isListening = False
				else:
					# Add received data to a buffer
					self.buffer.append(rcvbyte)
					
					if(rcvddata == False):
						self.log2file("Received data. Press OFFLINE button to dump to file.")
						sys.stdout.flush()
						rcvddata = True
			else:
				if(ibyte == 23):
					self.log2file("Printer is ONLINE")
					sys.stdout.flush()
					printerIsOnline = True

	############################################################
	# Process received data
	def processData(self):
		buf_line = ""
		gotCR = False
		gotESC = False
		gotUnder = False
		gotSubSupscrp = False
		wasSubscrp = False
		wasSupscrp = False
		gotPrintable = False
		self.haveESC = False
		ibyte = 0;
		self.bufferoutput.clear()

		for b in self.buffer:
			ibyte = int(b, 16)

			if gotESC: # Code after ESC
				if gotUnder:
					if ibyte == 48: # Cancel underlining
						if self.doHTML and self.haveESC:
							self.bufferoutput.append("</u>")
						gotUnder = False
						gotESC = False
					elif ibyte == 49: # Select underlining
						if self.doHTML and self.haveESC:
							self.bufferoutput.append("<u>")
						gotUnder = False
						gotESC = False
				elif gotSubSupscrp:
					if ibyte == 48: # Select Superscript
						if self.doHTML and self.haveESC:
							self.bufferoutput.append("<sup>")
						wasSupscrp = True
						gotSubSupscrp = False
						gotESC = False
					elif ibyte == 49: # Select Subscript
						if self.doHTML and self.haveESC:
							self.bufferoutput.append("<sub>")
						wasSubscrp = True
						gotSubSupscrp = False
						gotESC = False
				elif ibyte == 45: # Underlining
					gotUnder = True
				elif ibyte == 52: # Select italic mode
					if self.doHTML and self.haveESC:
						self.bufferoutput.append("<i>")
					elif self.doMarkdown and self.haveESC:
						self.bufferoutput.append("*")
					gotESC = False
				elif ibyte == 53: # Cancel italic mode
					if self.doHTML and self.haveESC:
						self.bufferoutput.append("</i>")
					elif self.doMarkdown and self.haveESC:
						self.bufferoutput.append("*")
					gotESC = False
				elif ibyte == 64: # Initialise printer
					gotESC = False
				elif ibyte == 69: # Select emphasised (bold) mode
					if self.doHTML and self.haveESC:
						self.bufferoutput.append("<b>")
					elif self.doMarkdown and self.haveESC:
						self.bufferoutput.append("**")
					gotESC = False
				elif ibyte == 70: # Cancel emphasised (bold) mode
					if self.doHTML and self.haveESC:
						self.bufferoutput.append("</b>")
					elif self.doMarkdown and self.haveESC:
						self.bufferoutput.append("**")
					gotESC = False
				elif ibyte == 71: # Select double strike mode
					if self.doHTML and self.haveESC:
						self.bufferoutput.append("<strike>")
					gotESC = False
				elif ibyte == 72: # Cancel double strike mode
					if self.doHTML and self.haveESC:
						self.bufferoutput.append("</strike>")
					gotESC = False
				elif ibyte == 83: # Subscript/Superscript
					gotSubSupscrp = True
				elif ibyte == 84: # Cancel Subscript/Superscript
					if wasSubscrp: # Cancel Subscript
						if self.doHTML and self.haveESC:
							self.bufferoutput.append("</sub>")
					elif wasSupscrp: # Cancel Superscript
						if self.doHTML and self.haveESC:
							self.bufferoutput.append("</sup>")
			elif ibyte >= 32 and ibyte <= 126: # Printable ASCII
				self.bufferoutput.append(chr(ibyte))
			elif ibyte == 13: # Carriage Return
				gotCR = True
			elif ibyte == 27: # ESC
				self.haveESC = True
				gotESC = True
			elif ibyte == 10 and gotCR: # Line Feed after CR
				if self.doHTML and self.haveESC:
					self.bufferoutput.append("<br>")
				else:
					self.bufferoutput.append("\n")
				gotCR = False
			else:
				self.bufferoutput.append(chr(ibyte))

	############################################################
	# Output to file and/or screen
	def generateOutput(self):
		if self.doFile:
			filename = datetime.datetime.fromtimestamp(time.time()).strftime('%Y%m%d-%H%M%S')

			if self.haveESC:
				if self.doHTML:
					self.log2file("Generating HTML file" + filename + ".html")
					outputfile = open(filename + ".html", "w")
					# Add HMTL Header to file
					outputfile.write("<html><head><title>Printed from Amstrad CPC with ACPCPE on " + filename + "</title></head><body>")
				elif self.doMarkdown:
					self.log2file("Generating Markdown file: " + filename + ".md")
					outputfile = open(filename + ".md", "w")
			else:
				self.log2file("Generating Text file: " + filename + ".txt")
				outputfile = open(filename + ".txt", "w")

			# Add body to file
			for bo in self.bufferoutput:
				outputfile.write(bo)
			
			if self.doHTML and self.haveESC:
				# Add HMTL Footer to file
				outputfile.write("</body></html>")

			outputfile.close()
		
		if self.doEcho:
			for bo in self.bufferoutput:
				print(bo, end='')
				#print(bo)
				sys.stdout.flush()
		
		# Generate RAW file
		if self.doRaw:
			outputraw = open(filename + ".raw", "w")
			for b in self.buffer:
				outputraw.write(b.decode('ascii'))
			outputraw.close()

	############################################################
	# Output to LOG file and screen
	def log2file(self, logstr):
		timestamp = datetime.datetime.fromtimestamp(time.time()).strftime('%d/%m/%Y %H:%M:%S')
		
		if(not self.beQuiet):
			print(timestamp + " - " + logstr)
			sys.stdout.flush()

		if(self.doLog):
			logfile = open("ACPCPE.log", "a")
			logfile.write("\n" + timestamp + " - " + logstr)
			logfile.close()


############################################################
# Program entry point when executed
if __name__ == '__main__':
	# Check that parameter have been received
	parser = argparse.ArgumentParser(prog="ACPCPE")
	parser.add_argument("port", help="serial port to listen from", type=str)
	group = parser.add_mutually_exclusive_group(required=False)
	group.add_argument("-e", "--echo", help="output data to screen too", action="store_true")
	group.add_argument("-q", "--quiet", help="suppress non-error messages", action="store_true")
	parser.add_argument("-nf", "--nofile", help="do not output to file", action="store_true")
	parser.add_argument("-nl", "--nolog", help="do not create a log file", action="store_true")
	parser.add_argument("-r", "--raw", help="generate a file (.raw) with the received hex values", action="store_true")
	parser.add_argument("-md", "--markdown", help="output file will be Markdown instead of HTML", action="store_true")
	parser.add_argument("-v", "--version", action="version", version="%(prog)s {version}".format(version=__version__))

	args = parser.parse_args()

	emul = acpcpe(args.port, args.echo, args.quiet, args.nofile, args.nolog, args.raw, args.markdown)
	emul.setserial()
	
	while True:
		emul.listen()
		if(len(emul.buffer) >0):
			emul.processData()
			emul.generateOutput()
