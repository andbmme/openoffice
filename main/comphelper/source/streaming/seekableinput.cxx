/**************************************************************
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * 
 *************************************************************/



// MARKER(update_precomp.py): autogen include statement, do not remove
#include "precompiled_comphelper.hxx"
#include <com/sun/star/io/XOutputStream.hpp>


#include <comphelper/seekableinput.hxx>

using namespace ::com::sun::star;

namespace comphelper
{

const sal_Int32 nConstBufferSize = 32000;

//---------------------------------------------------------------------------
void copyInputToOutput_Impl( const uno::Reference< io::XInputStream >& xIn,
							const uno::Reference< io::XOutputStream >& xOut )
{
	sal_Int32 nRead;
	uno::Sequence< sal_Int8 > aSequence( nConstBufferSize );

	do
	{
		nRead = xIn->readBytes( aSequence, nConstBufferSize );
		if ( nRead < nConstBufferSize )
		{
			uno::Sequence< sal_Int8 > aTempBuf( aSequence.getConstArray(), nRead );
			xOut->writeBytes( aTempBuf );
		}
		else
			xOut->writeBytes( aSequence );
	}
	while ( nRead == nConstBufferSize );
}

//---------------------------------------------------------------------------
OSeekableInputWrapper::OSeekableInputWrapper(
			const uno::Reference< io::XInputStream >& xInStream,
			const uno::Reference< lang::XMultiServiceFactory >& xFactory )
: m_xFactory( xFactory )
, m_xOriginalStream( xInStream )
{
	if ( !m_xFactory.is() )
		throw uno::RuntimeException();
}

//---------------------------------------------------------------------------
OSeekableInputWrapper::~OSeekableInputWrapper()
{
}

//---------------------------------------------------------------------------
uno::Reference< io::XInputStream > OSeekableInputWrapper::CheckSeekableCanWrap(
							const uno::Reference< io::XInputStream >& xInStream,
							const uno::Reference< lang::XMultiServiceFactory >& xFactory )
{
	// check that the stream is seekable and just wrap it if it is not
	uno::Reference< io::XSeekable > xSeek( xInStream, uno::UNO_QUERY );
	if ( xSeek.is() )
		return xInStream;

	uno::Reference< io::XInputStream > xNewStream( 
			static_cast< io::XInputStream* >( 
				new OSeekableInputWrapper( xInStream, xFactory ) ) );
    return xNewStream;
}

//---------------------------------------------------------------------------
void OSeekableInputWrapper::PrepareCopy_Impl()
{
	if ( !m_xCopyInput.is() )
	{
		if ( !m_xFactory.is() )
			throw uno::RuntimeException();

		uno::Reference< io::XOutputStream > xTempOut(
				m_xFactory->createInstance( ::rtl::OUString::createFromAscii( "com.sun.star.io.TempFile" ) ),
				uno::UNO_QUERY );

		if ( xTempOut.is() )
		{
			copyInputToOutput_Impl( m_xOriginalStream, xTempOut );
			xTempOut->closeOutput();

			uno::Reference< io::XSeekable > xTempSeek( xTempOut, uno::UNO_QUERY );
			if ( xTempSeek.is() )
			{
				xTempSeek->seek( 0 );
				m_xCopyInput = uno::Reference< io::XInputStream >( xTempOut, uno::UNO_QUERY );
				if ( m_xCopyInput.is() )
					m_xCopySeek = xTempSeek;
			}
		}
	}

	if ( !m_xCopyInput.is() )
		throw io::IOException();
}

// XInputStream
//---------------------------------------------------------------------------
sal_Int32 SAL_CALL OSeekableInputWrapper::readBytes( uno::Sequence< sal_Int8 >& aData, sal_Int32 nBytesToRead )
	throw ( io::NotConnectedException,
			io::BufferSizeExceededException,
			io::IOException,
			uno::RuntimeException )
{
	::osl::MutexGuard aGuard( m_aMutex );

	if ( !m_xOriginalStream.is() )
		throw io::NotConnectedException();

	PrepareCopy_Impl();

	return m_xCopyInput->readBytes( aData, nBytesToRead );
}

//---------------------------------------------------------------------------
sal_Int32 SAL_CALL OSeekableInputWrapper::readSomeBytes( uno::Sequence< sal_Int8 >& aData, sal_Int32 nMaxBytesToRead )
	throw ( io::NotConnectedException,
			io::BufferSizeExceededException,
			io::IOException,
			uno::RuntimeException )
{
	::osl::MutexGuard aGuard( m_aMutex );

	if ( !m_xOriginalStream.is() )
		throw io::NotConnectedException();

	PrepareCopy_Impl();

	return m_xCopyInput->readSomeBytes( aData, nMaxBytesToRead );
}

//---------------------------------------------------------------------------
void SAL_CALL OSeekableInputWrapper::skipBytes( sal_Int32 nBytesToSkip )
	throw ( io::NotConnectedException,
			io::BufferSizeExceededException,
			io::IOException,
			uno::RuntimeException )
{
	::osl::MutexGuard aGuard( m_aMutex );

	if ( !m_xOriginalStream.is() )
		throw io::NotConnectedException();

	PrepareCopy_Impl();

	m_xCopyInput->skipBytes( nBytesToSkip );
}

//---------------------------------------------------------------------------
sal_Int32 SAL_CALL OSeekableInputWrapper::available()
	throw ( io::NotConnectedException,
			io::IOException,
			uno::RuntimeException )
{
	::osl::MutexGuard aGuard( m_aMutex );

	if ( !m_xOriginalStream.is() )
		throw io::NotConnectedException();

	PrepareCopy_Impl();

	return m_xCopyInput->available();
}

//---------------------------------------------------------------------------
void SAL_CALL OSeekableInputWrapper::closeInput()
	throw ( io::NotConnectedException,
			io::IOException,
			uno::RuntimeException )
{
	::osl::MutexGuard aGuard( m_aMutex );

	if ( !m_xOriginalStream.is() )
		throw io::NotConnectedException();

	m_xOriginalStream->closeInput();
	m_xOriginalStream = uno::Reference< io::XInputStream >();

	if ( m_xCopyInput.is() )
	{
		m_xCopyInput->closeInput();
		m_xCopyInput = uno::Reference< io::XInputStream >();
	}

	m_xCopySeek = uno::Reference< io::XSeekable >();
}


// XSeekable
//---------------------------------------------------------------------------
void SAL_CALL OSeekableInputWrapper::seek( sal_Int64 location )
	throw ( lang::IllegalArgumentException,
			io::IOException,
			uno::RuntimeException )
{
	::osl::MutexGuard aGuard( m_aMutex );

	if ( !m_xOriginalStream.is() )
		throw io::NotConnectedException();

	PrepareCopy_Impl();

	m_xCopySeek->seek( location );
}

//---------------------------------------------------------------------------
sal_Int64 SAL_CALL OSeekableInputWrapper::getPosition()
	throw ( io::IOException,
			uno::RuntimeException )
{
	::osl::MutexGuard aGuard( m_aMutex );

	if ( !m_xOriginalStream.is() )
		throw io::NotConnectedException();

	PrepareCopy_Impl();

	return m_xCopySeek->getPosition();
}

//---------------------------------------------------------------------------
sal_Int64 SAL_CALL OSeekableInputWrapper::getLength()
	throw ( io::IOException,
			uno::RuntimeException )
{
	::osl::MutexGuard aGuard( m_aMutex );

	if ( !m_xOriginalStream.is() )
		throw io::NotConnectedException();

	PrepareCopy_Impl();

	return m_xCopySeek->getLength();
}

}	// namespace comphelper

