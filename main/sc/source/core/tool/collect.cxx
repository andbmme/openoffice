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
#include "precompiled_sc.hxx"



#include <string.h>
#include <tools/stream.hxx>
#include <unotools/transliterationwrapper.hxx>

#include "rechead.hxx"
#include "collect.hxx"
#include "document.hxx"			// fuer TypedStrData Konstruktor

// -----------------------------------------------------------------------

ScDataObject::~ScDataObject()
{
}

//------------------------------------------------------------------------
// Collection
//------------------------------------------------------------------------

void lcl_DeleteScDataObjects( ScDataObject** p, sal_uInt16 nCount )
{
	if ( p )
	{
		for (sal_uInt16 i = 0; i < nCount; i++) delete p[i];
		delete[] p;
		p = NULL;
	}
}

ScCollection::ScCollection(sal_uInt16 nLim, sal_uInt16 nDel) :
	nCount ( 0 ),
	nLimit ( nLim ),
	nDelta ( nDel ),
	pItems ( NULL )
{
	if (nDelta > MAXDELTA)
		nDelta = MAXDELTA;
	else if (nDelta == 0)
		nDelta = 1;
	if (nLimit > MAXCOLLECTIONSIZE)
		nLimit = MAXCOLLECTIONSIZE;
	else if (nLimit < nDelta)
		nLimit = nDelta;
	pItems = new ScDataObject*[nLimit];
}

ScCollection::ScCollection(const ScCollection& rCollection)
    :   ScDataObject(),
        nCount ( 0 ),
		nLimit ( 0 ),
		nDelta ( 0 ),
		pItems ( NULL )
{
	*this = rCollection;
}

//------------------------------------------------------------------------

ScCollection::~ScCollection()
{
	lcl_DeleteScDataObjects( pItems, nCount );
}

//------------------------------------------------------------------------
sal_uInt16 ScCollection::GetCount() const { return nCount; }
void ScCollection::AtFree(sal_uInt16 nIndex)
{
	if ((pItems) && (nIndex < nCount))
	{
		delete pItems[nIndex];
		--nCount;				// before memmove
		memmove ( &pItems[nIndex], &pItems[nIndex + 1], (nCount - nIndex) * sizeof(ScDataObject*));
		pItems[nCount] = NULL;
	}
}

//------------------------------------------------------------------------

void ScCollection::Free(ScDataObject* pScDataObject)
{
	AtFree(IndexOf(pScDataObject));
}

//------------------------------------------------------------------------

void ScCollection::FreeAll()
{
	lcl_DeleteScDataObjects( pItems, nCount );
	nCount = 0;
	pItems = new ScDataObject*[nLimit];
}

//------------------------------------------------------------------------

sal_Bool ScCollection::AtInsert(sal_uInt16 nIndex, ScDataObject* pScDataObject)
{
	if ((nCount < MAXCOLLECTIONSIZE) && (nIndex <= nCount) && pItems)
	{
		if (nCount == nLimit)
		{
			ScDataObject** pNewItems = new ScDataObject*[nLimit + nDelta];
			if (!pNewItems)
				return sal_False;
            nLimit = sal::static_int_cast<sal_uInt16>( nLimit + nDelta );
			memmove(pNewItems, pItems, nCount * sizeof(ScDataObject*));
			delete[] pItems;
			pItems = pNewItems;
		}
		if (nCount > nIndex)
			memmove(&pItems[nIndex + 1], &pItems[nIndex], (nCount - nIndex) * sizeof(ScDataObject*));
		pItems[nIndex] = pScDataObject;
		nCount++;
		return sal_True;
	}
	return sal_False;
}

//------------------------------------------------------------------------

sal_Bool ScCollection::Insert(ScDataObject* pScDataObject)
{
	return AtInsert(nCount, pScDataObject);
}

//------------------------------------------------------------------------

ScDataObject* ScCollection::At(sal_uInt16 nIndex) const
{
	if (nIndex < nCount)
		return pItems[nIndex];
	else
		return NULL;
}

//------------------------------------------------------------------------

sal_uInt16 ScCollection::IndexOf(ScDataObject* pScDataObject) const
{
	sal_uInt16 nIndex = 0xffff;
	for (sal_uInt16 i = 0; ((i < nCount) && (nIndex == 0xffff)); i++)
	{
		if (pItems[i] == pScDataObject) nIndex = i;
	}
	return nIndex;
}

//------------------------------------------------------------------------

ScCollection& ScCollection::operator=( const ScCollection& r )
{
	lcl_DeleteScDataObjects( pItems, nCount );

	nCount = r.nCount;
	nLimit = r.nLimit;
	nDelta = r.nDelta;
	pItems = new ScDataObject*[nLimit];
	for ( sal_uInt16 i=0; i<nCount; i++ )
		pItems[i] = r.pItems[i]->Clone();

	return *this;
}

//------------------------------------------------------------------------

ScDataObject*	ScCollection::Clone() const
{
	return new ScCollection(*this);
}

//------------------------------------------------------------------------
// ScSortedCollection
//------------------------------------------------------------------------

ScSortedCollection::ScSortedCollection(sal_uInt16 nLim, sal_uInt16 nDel, sal_Bool bDup) :
	ScCollection (nLim, nDel),
	bDuplicates ( bDup)
{
}

//------------------------------------------------------------------------

sal_uInt16 ScSortedCollection::IndexOf(ScDataObject* pScDataObject) const
{
	sal_uInt16 nIndex;
	if (Search(pScDataObject, nIndex))
		return nIndex;
	else
		return 0xffff;
}

//------------------------------------------------------------------------

sal_Bool ScSortedCollection::Search(ScDataObject* pScDataObject, sal_uInt16& rIndex) const
{
	rIndex = nCount;
	sal_Bool bFound = sal_False;
	short nLo = 0;
	short nHi = nCount - 1;
	short nIndex;
	short nCompare;
	while (nLo <= nHi)
	{
		nIndex = (nLo + nHi) / 2;
		nCompare = Compare(pItems[nIndex], pScDataObject);
		if (nCompare < 0)
			nLo = nIndex + 1;
		else
		{
			nHi = nIndex - 1;
			if (nCompare == 0)
			{
				bFound = sal_True;
				nLo = nIndex;
			}
		}
	}
	rIndex = nLo;
	return bFound;
}

//------------------------------------------------------------------------

sal_Bool ScSortedCollection::Insert(ScDataObject* pScDataObject)
{
	sal_uInt16 nIndex;
	sal_Bool bFound = Search(pScDataObject, nIndex);
	if (bFound)
	{
		if (bDuplicates)
			return AtInsert(nIndex, pScDataObject);
		else
			return sal_False;
	}
	else
		return AtInsert(nIndex, pScDataObject);
}

//------------------------------------------------------------------------

sal_Bool ScSortedCollection::InsertPos(ScDataObject* pScDataObject, sal_uInt16& nIndex)
{
	sal_Bool bFound = Search(pScDataObject, nIndex);
	if (bFound)
	{
		if (bDuplicates)
			return AtInsert(nIndex, pScDataObject);
		else
			return sal_False;
	}
	else
		return AtInsert(nIndex, pScDataObject);
}

//------------------------------------------------------------------------

sal_Bool ScSortedCollection::operator==(const ScSortedCollection& rCmp) const
{
	if ( nCount != rCmp.nCount )
		return sal_False;
	for (sal_uInt16 i=0; i<nCount; i++)
		if ( !IsEqual(pItems[i],rCmp.pItems[i]) )
			return sal_False;
	return sal_True;
}

//------------------------------------------------------------------------

//	IsEqual - komplette Inhalte vergleichen

sal_Bool ScSortedCollection::IsEqual(ScDataObject* pKey1, ScDataObject* pKey2) const
{
	return ( Compare(pKey1, pKey2) == 0 );		// Default: nur Index vergleichen
}

//------------------------------------------------------------------------

ScDataObject*	StrData::Clone() const
{
	return new StrData(*this);
}

//------------------------------------------------------------------------

short ScStrCollection::Compare(ScDataObject* pKey1, ScDataObject* pKey2) const
{
	StringCompare eComp = ((StrData*)pKey1)->aStr.CompareTo(((StrData*)pKey2)->aStr);
	if (eComp == COMPARE_EQUAL)
		return 0;
	else if (eComp == COMPARE_LESS)
		return -1;
	else
		return 1;
}

//------------------------------------------------------------------------

ScDataObject*	ScStrCollection::Clone() const
{
	return new ScStrCollection(*this);
}

//------------------------------------------------------------------------
// TypedScStrCollection
//------------------------------------------------------------------------

//UNUSED2008-05  TypedStrData::TypedStrData( ScDocument* pDoc, SCCOL nCol, SCROW nRow, SCTAB nTab,
//UNUSED2008-05                                  sal_Bool bAllStrings )
//UNUSED2008-05  {
//UNUSED2008-05      if ( pDoc->HasValueData( nCol, nRow, nTab ) )
//UNUSED2008-05      {
//UNUSED2008-05          pDoc->GetValue( nCol, nRow, nTab, nValue );
//UNUSED2008-05          if (bAllStrings)
//UNUSED2008-05              pDoc->GetString( nCol, nRow, nTab, aStrValue );
//UNUSED2008-05          nStrType = 0;
//UNUSED2008-05      }
//UNUSED2008-05      else
//UNUSED2008-05      {
//UNUSED2008-05          pDoc->GetString( nCol, nRow, nTab, aStrValue );
//UNUSED2008-05          nValue = 0.0;
//UNUSED2008-05          nStrType = 1;       //! Typ uebergeben ?
//UNUSED2008-05      }
//UNUSED2008-05  }


ScDataObject*	TypedStrData::Clone() const
{
	return new TypedStrData(*this);
}

TypedScStrCollection::TypedScStrCollection( sal_uInt16 nLim , sal_uInt16 nDel , sal_Bool bDup  )
	: ScSortedCollection( nLim, nDel, bDup ) 
{
	bCaseSensitive = sal_False; 
}

TypedScStrCollection::~TypedScStrCollection()
{}
ScDataObject* TypedScStrCollection::Clone() const
{
	return new TypedScStrCollection(*this);
}

TypedStrData*	 TypedScStrCollection::operator[]( const sal_uInt16 nIndex) const
{ 
	return (TypedStrData*)At(nIndex); 
}

void	TypedScStrCollection::SetCaseSensitive( sal_Bool bSet )		
{ 
	bCaseSensitive = bSet; 
}
	
short TypedScStrCollection::Compare( ScDataObject* pKey1, ScDataObject* pKey2 ) const
{
	short nResult = 0;

	if ( pKey1 && pKey2 )
	{
		TypedStrData& rData1 = (TypedStrData&)*pKey1;
		TypedStrData& rData2 = (TypedStrData&)*pKey2;

		if ( rData1.nStrType > rData2.nStrType )
			nResult = 1;
		else if ( rData1.nStrType < rData2.nStrType )
			nResult = -1;
		else if ( !rData1.nStrType /* && !rData2.nStrType */ )
		{
			//--------------------
			// Zahlen vergleichen:
			//--------------------
			if ( rData1.nValue == rData2.nValue )
				nResult = 0;
			else if ( rData1.nValue < rData2.nValue )
				nResult = -1;
			else
				nResult = 1;
		}
		else /* if ( rData1.nStrType && rData2.nStrType ) */
		{
			//---------------------
			// Strings vergleichen:
			//---------------------
			if ( bCaseSensitive )
                nResult = (short) ScGlobal::GetCaseTransliteration()->compareString(
					rData1.aStrValue, rData2.aStrValue );
			else
                nResult = (short) ScGlobal::GetpTransliteration()->compareString(
					rData1.aStrValue, rData2.aStrValue );
		}
	}

	return nResult;
}

sal_Bool TypedScStrCollection::FindText( const String& rStart, String& rResult,
									sal_uInt16& rPos, sal_Bool bBack ) const
{
	//	Die Collection ist nach String-Vergleichen sortiert, darum muss hier
	//	alles durchsucht werden

	sal_Bool bFound = sal_False;

	String aOldResult;
	if ( rPos != SCPOS_INVALID && rPos < nCount )
	{
		TypedStrData* pData = (TypedStrData*) pItems[rPos];
		if (pData->nStrType)
			aOldResult = pData->aStrValue;
	}

	if ( bBack )									// rueckwaerts
	{
		sal_uInt16 nStartPos = nCount;
		if ( rPos != SCPOS_INVALID )
			nStartPos = rPos;						// weitersuchen...

		for ( sal_uInt16 i=nStartPos; i>0; )
		{
			--i;
			TypedStrData* pData = (TypedStrData*) pItems[i];
			if (pData->nStrType)
			{
                if ( ScGlobal::GetpTransliteration()->isMatch( rStart, pData->aStrValue ) )
				{
					//	If the collection is case sensitive, it may contain several entries
					//	that are equal when compared case-insensitive. They are skipped here.
					if ( !bCaseSensitive || !aOldResult.Len() ||
                            !ScGlobal::GetpTransliteration()->isEqual(
                            pData->aStrValue, aOldResult ) )
					{
						rResult = pData->aStrValue;
						rPos = i;
						bFound = sal_True;
						break;
					}
				}
			}
		}
	}
	else											// vorwaerts
	{
		sal_uInt16 nStartPos = 0;
		if ( rPos != SCPOS_INVALID )
			nStartPos = rPos + 1;					// weitersuchen...

		for ( sal_uInt16 i=nStartPos; i<nCount; i++ )
		{
			TypedStrData* pData = (TypedStrData*) pItems[i];
			if (pData->nStrType)
			{
                if ( ScGlobal::GetpTransliteration()->isMatch( rStart, pData->aStrValue ) )
				{
					//	If the collection is case sensitive, it may contain several entries
					//	that are equal when compared case-insensitive. They are skipped here.
					if ( !bCaseSensitive || !aOldResult.Len() ||
                            !ScGlobal::GetpTransliteration()->isEqual(
                            pData->aStrValue, aOldResult ) )
					{
						rResult = pData->aStrValue;
						rPos = i;
						bFound = sal_True;
						break;
					}
				}
			}
		}
	}

	return bFound;
}

		// Gross-/Kleinschreibung anpassen

sal_Bool TypedScStrCollection::GetExactMatch( String& rString ) const
{
	for (sal_uInt16 i=0; i<nCount; i++)
	{
		TypedStrData* pData = (TypedStrData*) pItems[i];
        if ( pData->nStrType && ScGlobal::GetpTransliteration()->isEqual(
                pData->aStrValue, rString ) )
		{
			rString = pData->aStrValue;							// String anpassen
			return sal_True;
		}
	}

	return sal_False;
}



