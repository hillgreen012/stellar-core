// Copyright 2014 Stellar Development Foundation and contributors. Licensed
// under the ISC License. See the COPYING file at the top-level directory of
// this distribution or at http://opensource.org/licenses/ISC

#include "main/Application.h"
#include "TransactionFrame.h"
#include "xdrpp/marshal.h"
#include <string>
#include "lib/json/json.h"
#include "util/Logging.h"
#include "ledger/LedgerDelta.h"
#include "ledger/OfferFrame.h"

namespace stellar
{

    TransactionFrame::pointer TransactionFrame::makeTransactionFromWire(TransactionEnvelope const& msg)
{
    //mSignature = msg.signature;

	// SANITY check sig
    return TransactionFrame::pointer();
}

    
// TODO.2 we can probably get rid of this
uint256& TransactionFrame::getSignature()
{
    return mEnvelope.signature;
}

uint256& TransactionFrame::getHash()
{
	if(isZero(mHash))
	{
        Transaction tx;
        toXDR(tx);
        xdr::msg_ptr xdrBytes(xdr::xdr_to_msg(tx));
        hashXDR(std::move(xdrBytes), mHash);
	}
	return(mHash);
}


int64_t TransactionFrame::getTransferRate(Currency& currency, LedgerMaster& ledgerMaster)
{
    if(currency.native()) return TRANSFER_RATE_DIVISOR;

    AccountFrame issuer;
    ledgerMaster.getDatabase().loadAccount(currency.ci().issuer, issuer);
    return issuer.mEntry.account().transferRate;    
}



/*
// returns true if this account can hold this currency
bool Transaction::isAuthorizedToHold(const AccountEntry& account, 
    const CurrencyIssuer& ci, LedgerMaster& ledgerMaster)
{
    LedgerEntry issuer;
    if(ledgerMaster.getDatabase().loadAccount(ci.issuer,issuer))
    {
        if(issuer.account().flags && AccountEntry::AUTH_REQUIRED_FLAG)
        {

        } else return true;
    } else return false;
}
*/
    

// take fee
// check seq
// take seq
// check max ledger
bool TransactionFrame::preApply(TxDelta& delta,LedgerMaster& ledgerMaster)
{
    if(mSigningAccount.mEntry.account().sequence != mEnvelope.tx.seqNum)
    {
        mResultCode = txBADSEQ;
        CLOG(ERROR, "Tx") << "Tx sequence # doesn't match Account in validated set. This should never happen.";
        return false;
    }

    if((ledgerMaster.getCurrentLedger()->mHeader.ledgerSeq > mEnvelope.tx.maxLedger) ||
        (ledgerMaster.getCurrentLedger()->mHeader.ledgerSeq < mEnvelope.tx.minLedger))
    {
        mResultCode = txMALFORMED;
        CLOG(ERROR, "Tx") << "tx not in valid ledger in validated set. This should never happen.";
        return false;
    }
        
    uint32_t fee=ledgerMaster.getCurrentLedger()->getTxFee();
    if(fee > mEnvelope.tx.maxFee)
    {
        mResultCode = txNOFEE;
        CLOG(ERROR, "Tx") << "tx isn't willing to pay fee. This should never happen.";
        return false;
    }


    if(mSigningAccount.mEntry.account().balance < fee)
    {
        mResultCode = txNOFEE;
        CLOG(ERROR, "Tx") << "tx doesn't have fee. This should never happen.";
        // TODO.2: what do we do here? take all their balance?
        return false;
    }

    delta.setStart(mSigningAccount);

    mSigningAccount.mEntry.account().balance -= fee;
    mSigningAccount.mEntry.account().sequence += 1;

    delta.setFinal(mSigningAccount);
    return true;
}

void TransactionFrame::apply(TxDelta& delta, LedgerMaster& ledgerMaster)
{
    if(ledgerMaster.getDatabase().loadAccount(mEnvelope.tx.account, mSigningAccount))
    {
        if(preApply(delta,ledgerMaster))
        {
            doApply(delta,ledgerMaster);
        }
    } else
    {
        CLOG(ERROR, "Tx") << "Signing account not found. This should never happen";
    }
    
}

// called when determining if we should accept this tx.
// make sure sig is correct
// make sure maxFee is above the current fee
// make sure it is in the correct ledger bounds
bool TransactionFrame::checkValid(Application& app)
{
    if(mEnvelope.tx.maxFee < app.getLedgerGateway().getFee()) return false;
    if(mEnvelope.tx.maxLedger < app.getLedgerGateway().getLedgerNum()) return false;
    if(mEnvelope.tx.minLedger > app.getLedgerGateway().getLedgerNum()) return false;
    
    return doCheckValid(app);
}

void TransactionFrame::toXDR(Transaction& envelope)
{
    // LATER
}

void TransactionFrame::toXDR(TransactionEnvelope& envelope)
{
    // LATER
}

StellarMessage&& TransactionFrame::toStellarMessage()
{
    StellarMessage msg;
    msg.type(TRANSACTION);
    toXDR(msg.transaction());
    return std::move(msg);
}
}