import React from 'react';
import { Routes, Route, Navigate } from 'react-router-dom';
import { LanguageProvider } from './context/LanguageContext';
import ElderlyAlertSystem from './components/ElderlyAlertSystem';
import ElderlyLogin from './components/ElderlyLogin';
import ElderlyAuth from './components/ElderlyAuth';
import ElderlyDashboard from './components/ElderlyDashboard';
import ElderlyHome from './components/ElderlyHome';
import FallAlert from './components/FallAlert';
import HealthPage from './pages/HealthPage';
import AssistantPage from './pages/AssistantPage';
import ElderlyProfilePage from './pages/ElderlyProfilePage';
import GuardianAuthPage from './pages/GuardianAuthPage';
import GuardianDashboardPage from './pages/GuardianDashboardPage';
import PortalSelectionPage from './pages/PortalSelectionPage';

function App() {
  return (
    <LanguageProvider>
      <ElderlyAlertSystem>
        <Routes>
          {/* Portal Selection Landing Route */}
          <Route path="/" element={<PortalSelectionPage />} />
          <Route path="/portal" element={<PortalSelectionPage />} />

          {/* Elderly Routes */}
          <Route path="/login" element={<ElderlyLogin />} />
          <Route path="/register" element={<ElderlyAuth />} />
          <Route path="/dashboard" element={<ElderlyDashboard />} />
          <Route path="/home" element={<ElderlyHome />} />
          <Route path="/health" element={<HealthPage />} />
          <Route path="/assistant" element={<AssistantPage />} />
          <Route path="/profile" element={<ElderlyProfilePage />} />
          <Route path="/elderly-profile" element={<ElderlyProfilePage />} />
          <Route path="/fall-alert" element={<FallAlert />} />

          {/* Guardian Routes */}
          <Route path="/guardian-auth" element={<GuardianAuthPage />} />
          <Route path="/guardian-login" element={<GuardianAuthPage />} />
          <Route path="/guardian-dashboard" element={<GuardianDashboardPage />} />

          {/* Fallback Catch-all Route */}
          <Route path="*" element={<Navigate to="/" replace />} />
        </Routes>
      </ElderlyAlertSystem>
    </LanguageProvider>
  );
}

export default App;
