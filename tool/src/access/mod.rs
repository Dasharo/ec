// SPDX-License-Identifier: MIT

use crate::Error;
use downcast_rs::Downcast;

#[cfg(not(feature = "std"))]
use alloc::boxed::Box;

#[cfg(feature = "hidapi")]
pub use self::hid::AccessHid;
#[cfg(feature = "hidapi")]
mod hid;

#[cfg(any(
    feature = "redox_hwio",
    all(feature = "std", target_os = "linux")
))]
pub use self::lpc::*;
#[cfg(any(
    feature = "redox_hwio",
    all(feature = "std", target_os = "linux")
))]
mod lpc;

/// Access method for running an EC command
pub trait Access: Downcast + Send + 'static {
    /// Sends a command using the access method. Only internal use is recommended
    unsafe fn command(&mut self, cmd: u8, data: &mut [u8]) -> Result<u8, Error>;

    /// The maximum size that can be provided for the data argument
    fn data_size(&self) -> usize;

    /// Read from the debug space
    //TODO: better public interface
    unsafe fn read_debug(&mut self, _addr: u8) -> Result<u8, Error> {
        Err(Error::NotSupported)
    }

    /// Read from the ACPI EC RAM
    unsafe fn acpi_read(&mut self, _addr: u8) -> Result<u8, Error> {
        Err(Error::NotSupported)
    }

    /// Write to the ACPI EC RAM
    unsafe fn acpi_write(&mut self, _addr: u8, _data: u8) -> Result<(), Error> {
        Err(Error::NotSupported)
    }
}

impl Access for Box<dyn Access> {
    unsafe fn command(&mut self, cmd: u8, data: &mut [u8]) -> Result<u8, Error> {
        (**self).command(cmd, data)
    }

    fn data_size(&self) -> usize {
        (**self).data_size()
    }

    unsafe fn read_debug(&mut self, addr: u8) -> Result<u8, Error> {
        (**self).read_debug(addr)
    }

    unsafe fn acpi_read(&mut self, addr: u8) -> Result<u8, Error> {
        (**self).acpi_read(addr)
    }

    unsafe fn acpi_write(&mut self, addr: u8, data: u8) -> Result<(), Error> {
        (**self).acpi_write(addr, data)
    }
}

downcast_rs::impl_downcast!(Access);
