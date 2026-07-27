import React from 'react';
import { Routes, Route, Navigate } from 'react-router-dom';
import ElderlyLogin from './components/ElderlyLogin';
import ElderlyAuth from './components/ElderlyAuth';
import ElderlyDashboard from './components/ElderlyDashboard';
import ElderlyHome from './components/ElderlyHome';
import FallAlert from './components/FallAlert';
import GuardianAuthPage from './pages/GuardianAuthPage';
import GuardianDashboardPage from './pages/GuardianDashboardPage';

function App() {
  const isGuardianLoggedIn = !!localStorage.getItem('guardian_username');
  const isElderlyLoggedIn = !!localStorage.getItem('elderly_id');

  return (
    <Routes>
      {/* Elderly Routes */}
      <Route path="/login" element={<ElderlyLogin />} />
      <Route path="/register" element={<ElderlyAuth />} />
      <Route path="/dashboard" element={<ElderlyDashboard />} />
      <Route path="/home" element={<ElderlyHome />} />
      <Route path="/fall-alert" element={<FallAlert />} />

      {/* Guardian Routes */}
      <Route path="/guardian-auth" element={<GuardianAuthPage />} />
      <Route path="/guardian-login" element={<GuardianAuthPage />} />
      <Route path="/guardian-dashboard" element={<GuardianDashboardPage />} />
      <Route path="/portal" element={<Navigate to="/guardian-auth" replace />} />

      {/* Default Landing Route */}
      <Route
        path="/"
        element={
          isGuardianLoggedIn ? (
            <Navigate to="/guardian-dashboard" replace />
          ) : isElderlyLoggedIn ? (
            <Navigate to="/dashboard" replace />
          ) : (
            <ElderlyLogin />
          )
        }
      />

      {/* Fallback Catch-all Route */}
      <Route path="*" element={<Navigate to="/" replace />} />
    </Routes>
  );
}

export default App;
